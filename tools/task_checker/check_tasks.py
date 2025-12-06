import sys
import os
from pathlib import Path
import clang.cindex
from clang.cindex import CursorKind, TypeKind
import typer
from typing import List, Optional
import subprocess
from rich.console import Console
from rich.table import Table
from rich.syntax import Syntax
from rich.panel import Panel

app = typer.Typer()
console = Console()

def get_clang_resource_dir():
    try:
        result = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except Exception:
        return None

def get_system_include_paths():
    try:
        # Try to get default include paths from the compiler
        # We use c++ as a generic driver, or clang++ if available
        driver = "clang++"
        try:
            subprocess.run([driver, "--version"], capture_output=True, check=True)
        except FileNotFoundError:
            driver = "c++"

        result = subprocess.run(
            [driver, "-E", "-x", "c++", "-", "-v"],
            input="",
            capture_output=True,
            text=True
        )
        output = result.stderr  # GCC/Clang print verbose info to stderr

        includes = []
        in_search_list = False
        for line in output.splitlines():
            if "#include <...> search starts here:" in line:
                in_search_list = True
                continue
            if "End of search list." in line:
                break
            if in_search_list:
                path = line.strip()
                if os.path.exists(path):
                    includes.append(path)
        return includes
    except Exception:
        return []

def is_task_type(type_obj: clang.cindex.Type) -> bool:
    """Check if the type is a Task or koroutine::Task."""
    canonical = type_obj.get_canonical()
    spelling = canonical.spelling
    return "Task<" in spelling or "koroutine::Task" in spelling

def find_violations(cursor: clang.cindex.Cursor, filename: str, violations: List[dict]):
    """Recursively traverse AST to find violations."""

    if cursor.location.file and cursor.location.file.name != filename:
        return

    if cursor.kind == CursorKind.COMPOUND_STMT:
        for child in cursor.get_children():
            node = child
            while node.kind.name == 'EXPR_WITH_CLEANUPS':
                children = list(node.get_children())
                if children:
                    node = children[0]
                else:
                    break

            if node.kind.name in ['CALL_EXPR', 'CXX_MEMBER_CALL_EXPR']:
                # Ignore operator= calls (assignments)
                if "operator=" in node.spelling:
                    continue

                # Ignore std::move and std::forward
                if "std::move" in node.spelling or "std::forward" in node.spelling:
                    continue

                if is_task_type(node.type):
                    violations.append({
                        "file": filename,
                        "line": node.location.line,
                        "col": node.location.column,
                        "message": "Task returned by function call is ignored (missing co_await?)",
                        "code": get_cursor_text(node)
                    })

    if cursor.kind.name == 'CO_RETURN_STMT':
        children = list(cursor.get_children())
        if children:
            expr = children[0]
            while expr.kind.name in ['UNEXPOSED_EXPR', 'EXPR_WITH_CLEANUPS', 'IMPLICIT_CAST_EXPR', 'CXX_FUNCTIONAL_CAST_EXPR']:
                sub = list(expr.get_children())
                if sub:
                    expr = sub[0]
                else:
                    break

            if is_task_type(expr.type):
                 violations.append({
                        "file": filename,
                        "line": cursor.location.line,
                        "col": cursor.location.column,
                        "message": "co_return returning a Task directly. Did you mean 'co_return co_await ...'?",
                        "code": get_cursor_text(cursor)
                    })

    for child in cursor.get_children():
        find_violations(child, filename, violations)

def get_cursor_text(cursor):
    if not cursor.extent.start.file:
        return ""

    try:
        with open(cursor.extent.start.file.name, 'r', encoding='utf-8') as f:
            f.seek(cursor.extent.start.offset)
            return f.read(cursor.extent.end.offset - cursor.extent.start.offset)
    except Exception:
        return ""

@app.command()
def check(
    files: List[Path] = typer.Argument(..., help="C++ source files or directories to check"),
    compile_db: Path = typer.Option(Path("build"), help="Path to directory containing compile_commands.json"),
    libclang_path: Optional[str] = typer.Option(None, help="Path to libclang library file (e.g. libclang.so)")
):
    """
    Check C++ files for common coroutine mistakes.
    """
    if libclang_path:
        clang.cindex.Config.set_library_file(libclang_path)

    try:
        compdb = clang.cindex.CompilationDatabase.fromDirectory(str(compile_db))
    except clang.cindex.CompilationDatabaseError:
        console.print(f"[yellow]Warning: Could not load compile_commands.json from {compile_db}. Analysis might be inaccurate due to missing includes.[/yellow]")
        compdb = None

    index = clang.cindex.Index.create()

    resource_dir = get_clang_resource_dir()
    system_includes = get_system_include_paths()
    # console.print(f"[dim]System includes: {system_includes}[/dim]")

    system_args = []
    if resource_dir:
        system_args.extend(["-isystem", f"{resource_dir}/include"])

    for inc in system_includes:
        system_args.extend(["-isystem", inc])

    base_args = ["-std=c++20"] + system_args

    # Expand directories
    files_to_check = []
    for path in files:
        if path.is_dir():
            for ext in ['*.cpp', '*.cc', '*.cxx']:
                files_to_check.extend(path.rglob(ext))
        else:
            files_to_check.append(path)

    # Remove duplicates and sort
    files_to_check = sorted(list(set(files_to_check)))

    console.print(f"[bold]Found {len(files_to_check)} files to check.[/bold]")

    total_violations = 0

    for file_path in files_to_check:
        abs_path = file_path.resolve()
        console.print(f"Checking [bold blue]{abs_path}[/bold blue]...")

        args = list(base_args)

        if compdb:
            cmds = compdb.getCompileCommands(str(abs_path))
            if cmds:
                # console.print("[dim]Found in compile_commands.json[/dim]")
                cmd = cmds[0]
                raw_args = list(cmd.arguments)[1:]
                new_args = []
                skip = False
                for arg in raw_args:
                    if skip:
                        skip = False
                        continue
                    if arg == '-o':
                        skip = True
                        continue
                    if arg == '-c':
                        continue
                    if arg == '--':
                        continue
                    if arg == str(abs_path):
                        continue
                    new_args.append(arg)
                args = system_args + new_args
            else:
                # console.print("[dim]Not found in compile_commands.json, using fallback[/dim]")
                # Fallback: try to get include paths from the first entry in compdb
                # This is useful for header files not explicitly in the DB
                try:
                    all_cmds = compdb.getAllCompileCommands()
                    if all_cmds and len(all_cmds) > 0:
                        # Iterate through arguments of the first command to find include paths
                        first_cmd = all_cmds[0]
                        # Skip the compiler executable (first argument)
                        raw_args = list(first_cmd.arguments)[1:]
                        i = 0
                        while i < len(raw_args):
                            arg = raw_args[i]
                            if arg in ['-I', '-isystem']:
                                if i + 1 < len(raw_args):
                                    # Add both flag and path if not already present (simple check)
                                    if arg not in args or raw_args[i+1] not in args:
                                        args.extend([arg, raw_args[i+1]])
                                    i += 2
                                    continue
                            elif arg.startswith("-I") or arg.startswith("-isystem") or arg.startswith("-D") or arg.startswith("-std="):
                                if arg not in args:
                                    args.append(arg)
                            i += 1
                except Exception:
                    pass


        try:
            # console.print(f"[dim]Args: {args}[/dim]")
            tu = index.parse(str(abs_path), args=args)
        except Exception as e:
            console.print(f"[red]Error parsing {abs_path}: {e}[/red]")
            continue

        for diagnostic in tu.diagnostics:
            if diagnostic.severity >= clang.cindex.Diagnostic.Error:
                # Only show errors for the file being checked to reduce noise
                if diagnostic.location.file and diagnostic.location.file.name == str(abs_path):
                    console.print(f"[red]Parse Error: {diagnostic}[/red]")

        violations = []
        find_violations(tu.cursor, str(abs_path), violations)

        if violations:
            total_violations += len(violations)
            table = Table(title=f"Violations in {file_path.name}")
            table.add_column("Line", style="cyan", no_wrap=True)
            table.add_column("Message", style="magenta")
            table.add_column("Code", style="green")

            for v in violations:
                table.add_row(str(v['line']), v['message'], v['code'])

            console.print(table)
            console.print()

    if total_violations > 0:
        console.print(f"[bold red]Found {total_violations} violations![/bold red]")
        sys.exit(1)
    else:
        console.print("[bold green]No violations found![/bold green]")

if __name__ == "__main__":
    app()
