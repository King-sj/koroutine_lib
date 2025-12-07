import os
import subprocess
import sys

# Configuration
# Adjust this path to point to the cpp-httplib repository relative to this script
HTTPLIB_REL_PATH = "../../best_pro/cpp-httplib"
STATE_FILE = ".httplib_last_sync"

def get_current_commit(repo_path):
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo_path).decode().strip()
    except subprocess.CalledProcessError:
        print(f"Error: Could not get HEAD of {repo_path}")
        sys.exit(1)

def get_new_commits(repo_path, last_commit, current_commit):
    try:
        # Get commits between last_commit and current_commit
        # Format: hash - subject
        cmd = ["git", "log", f"{last_commit}..{current_commit}", "--pretty=format:%h - %s"]
        return subprocess.check_output(cmd, cwd=repo_path).decode().strip().split('\n')
    except subprocess.CalledProcessError:
        return []

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_path = os.path.abspath(os.path.join(script_dir, HTTPLIB_REL_PATH))
    state_file_path = os.path.join(script_dir, STATE_FILE)

    if not os.path.exists(repo_path):
        print(f"Error: cpp-httplib not found at {repo_path}")
        print("Please check the HTTPLIB_REL_PATH in the script.")
        return

    current_commit = get_current_commit(repo_path)

    if not os.path.exists(state_file_path):
        print(f"No sync state found. Creating {STATE_FILE} with current HEAD of cpp-httplib.")
        with open(state_file_path, "w", encoding="utf-8") as f:
            f.write(current_commit)
        return

    with open(state_file_path, "r", encoding="utf-8") as f:
        last_commit = f.read().strip()

    if last_commit == current_commit:
        print("No new updates in cpp-httplib.")
        return

    new_commits = get_new_commits(repo_path, last_commit, current_commit)

    # Filter out empty strings if any
    new_commits = [c for c in new_commits if c]

    if not new_commits:
        print("No new commits found (detached head or other issue?).")
        return

    print(f"Found {len(new_commits)} new commits in cpp-httplib since {last_commit[:7]}:")
    print("-" * 60)
    for commit in new_commits:
        print(commit)
    print("-" * 60)

    print("\nTo mark these as reviewed, run:")
    print(f"  echo {current_commit} > {state_file_path}")

    # Exit with error code to notify build system
    sys.exit(1)

if __name__ == "__main__":
    main()
