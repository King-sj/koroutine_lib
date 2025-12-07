import subprocess
import time
import re
import os

def build_release():
    print("Building in Release mode...")
    subprocess.check_call(["cmake", "-S", ".", "-B", "build_release", "-DCMAKE_BUILD_TYPE=Release"])
    subprocess.check_call(["cmake", "--build", "build_release", "--target", "http_server_bench", "-j"])

def start_server(port):
    print(f"Starting benchmark server on port {port}...")
    # Adjust path based on where script is run. Assuming run from project root.
    server_path = "./build_release/benchmark/http_server_bench"
    if not os.path.exists(server_path):
        # Try finding it in bin
        server_path = "./build_release/bin/http_server_bench"

    if not os.path.exists(server_path):
        raise FileNotFoundError(f"Could not find server executable at {server_path}")

    process = subprocess.Popen([server_path, str(port)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(2) # Wait for startup
    return process

def run_wrk(connections, duration, port):
    threads = 8
    if connections < 8:
        threads = connections

    print(f"Running wrk with {connections} connections and {threads} threads for {duration}s...")
    try:
        output = subprocess.check_output(
            ["wrk", f"-t{threads}", f"-c{connections}", f"-d{duration}s", f"http://localhost:{port}/hi"],
            text=True
        )
        return output
    except subprocess.CalledProcessError as e:
        print(f"Error running wrk: {e}")
        return None

def parse_wrk_output(output):
    qps = 0.0
    latency = "N/A"

    # Parse Requests/sec
    qps_match = re.search(r"Requests/sec:\s+(\d+\.?\d*)", output)
    if qps_match:
        qps = float(qps_match.group(1))

    # Parse Latency (Avg)
    # Output format: Latency    14.02ms    1.68ms  76.54ms   99.01%
    lat_match = re.search(r"Latency\s+(\d+\.?\d*\w+)", output)
    if lat_match:
        latency = lat_match.group(1)

    return qps, latency

def plot_results(concurrency_list, qps_list):
    try:
        import matplotlib.pyplot as plt
        plt.figure(figsize=(10, 6))
        plt.plot(concurrency_list, qps_list, marker='o', linestyle='-', color='b')
        plt.title('Koroutine HTTP Server Performance')
        plt.xlabel('Concurrency (Connections)')
        plt.ylabel('QPS (Requests/sec)')
        plt.grid(True)
        plt.savefig('benchmark_result.png')
        print("Chart saved to benchmark_result.png")
    except ImportError:
        print("matplotlib not found, skipping plot generation.")
    except Exception as e:
        print(f"Could not generate plot: {e}")

def main():
    port = 8081
    concurrency_levels = [10, 50, 100, 200, 500, 1000, 2000, 5000, 10000]
    duration = 10

    try:
        build_release()
    except Exception as e:
        print(f"Build failed: {e}")
        return

    server_process = None
    results = []

    try:
        server_process = start_server(port)

        print(f"{'Concurrency':<15} | {'QPS':<15} | {'Latency':<15}")
        print("-" * 50)

        for conn in concurrency_levels:
            output = run_wrk(conn, duration, port)
            if output:
                qps, latency = parse_wrk_output(output)
                print(f"{conn:<15} | {qps:<15.2f} | {latency:<15}")
                results.append((conn, qps))
            else:
                print(f"{conn:<15} | {'Failed':<15} | {'Failed':<15}")
                results.append((conn, 0))

    finally:
        if server_process:
            server_process.terminate()
            server_process.wait()
            print("Server stopped.")

    # Find max QPS
    if results:
        max_qps = max(results, key=lambda x: x[1])
        print("\n" + "="*30)
        print(f"Peak Performance: {max_qps[1]:.2f} QPS at {max_qps[0]} concurrency")
        print("="*30)

        # Plot
        conns = [r[0] for r in results]
        qpss = [r[1] for r in results]
        plot_results(conns, qpss)
    else:
        print("No results collected.")

if __name__ == "__main__":
    main()