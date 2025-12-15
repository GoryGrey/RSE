# Betti-RDL CI/CD Workflow Documentation

## Overview

The Betti-RDL project uses GitHub Actions for continuous integration and deployment. The CI/CD pipeline ensures code quality, performance, and multi-language compatibility on every commit and pull request.

## Workflow File

**Location**: `.github/workflows/ci.yml`

**Triggers**:
- `push` to branches: `main`, `develop`, `feat-*`
- `pull_request` to branches: `main`, `develop`

## Jobs Overview

### 1. Build and Test (`build-and-test`)

Builds the C++ kernel and runs unit tests across Release and Debug configurations.

**Matrix**:
- `build-type`: `[Release, Debug]`

**Steps**:

1. **Checkout Code**
   - Fetches the repository

2. **Install Dependencies**
   - Ubuntu build tools, CMake, libatomic, Python3, Node.js

3. **Configure CMake**
   - Generates build files for the specified build type

4. **Build C++ Kernel**
   - Compiles all targets using CMake

5. **Run Unit Tests**
   - Executes all test targets via `ctest`
   - Tests included:
     - `allocator_test` - Memory allocator validation
     - `fixed_structures_test` - Fixed data structure tests
     - `c_api_test` - C API compatibility tests
     - `threadsafe_scheduler_test` - Event scheduler thread-safety
     - `memory_telemetry_test` - Memory tracking accuracy

6. **Run Benchmark Harness**
   - Executes all three scenarios: Firehose, Deep Dive, Swarm
   - Generates JSON, CSV, and text reports
   - Validates performance baselines

7. **Run Stress Test**
   - Extended performance validation under sustained load

8. **Upload Benchmark Reports**
   - Stores results as GitHub Actions artifacts for analysis

**Expected Duration**: 3-5 minutes per build type

### 2. Sanitizer Checks (`sanitizer-checks`)

Validates memory safety using AddressSanitizer (ASAN) and LeakSanitizer (LSAN).

**Steps**:

1. **Configure CMake with Sanitizers**
   - Enables `-fsanitize=address -fsanitize=leak`
   - Compiles in Release mode with debug symbols

2. **Build with Sanitizers**
   - Produces binaries instrumented for memory safety checks

3. **Run mega_demo with ASAN/LSAN**
   - The three killer demos (Logistics, Neural Net, Contagion)
   - 60-second timeout to prevent hangs
   - Detects memory leaks and buffer overflows

4. **Run parallel_scaling_test with ASAN/LSAN**
   - Multi-threaded stress test
   - Validates thread safety and memory access patterns

5. **Run betti_rdl_stress_test with ASAN/LSAN**
   - Deep recursion and event processing stress test
   - Catches use-after-free and buffer issues

**Expected Duration**: 2-3 minutes

**Note**: ASAN/LSAN may be slower but catch subtle memory errors that Release builds miss.

### 3. Python Bindings (`python-bindings`)

Validates Python FFI bindings for multi-language compatibility.

**Steps**:

1. **Install Dependencies**
   - Python development headers, setuptools, wheel

2. **Build Python Bindings**
   - Compiles C++ extension module

3. **Smoke Test Python Bindings**
   - Runs `python/example.py`
   - Validates basic kernel creation and event processing
   - Checks FFI correctness

**Expected Duration**: 2-3 minutes

**What it tests**:
```python
import betti_rdl
kernel = betti_rdl.Kernel()
kernel.spawn_process(0, 0, 0)
kernel.inject_event(0, 0, 0, 1)
kernel.run(1000)
assert kernel.get_events_processed() > 0
```

### 4. Node.js Bindings (`nodejs-bindings`)

Validates Node.js N-API bindings for JavaScript compatibility.

**Steps**:

1. **Setup Node.js**
   - Version 18 LTS

2. **Install Dependencies**
   - npm install (node-gyp, native build tools)

3. **Build Node.js Bindings**
   - Compiles N-API native module

4. **Smoke Test Node.js Bindings**
   - Runs `nodejs/example.js`
   - Validates async kernel operations
   - Checks N-API correctness

**Expected Duration**: 2-3 minutes

**What it tests**:
```javascript
const { Kernel } = require('betti-rdl');
const k = new Kernel();
k.run(1000);
assert(k.getEventsProcessed() > 0);
```

### 5. Benchmark Comparison (`benchmark-comparison`)

Comprehensive benchmark suite with detailed reporting and PR comments.

**Steps**:

1. **Build Benchmark Suite**
   - Compiles `benchmark_harness` and related tools

2. **Run Full Benchmark Suite**
   - Executes Firehose, Deep Dive, and Swarm scenarios
   - Generates all output formats (JSON, CSV, text)

3. **Generate Benchmark Report Summary**
   - Creates markdown summary of results

4. **Comment PR with Benchmark Results**
   - Posts benchmark comparison to PR (if applicable)
   - Allows reviewers to see performance impact

5. **Upload Benchmark Results**
   - Stores results as artifacts for trend analysis

**Expected Duration**: 3-5 minutes

## Performance Baselines

The following thresholds are validated:

### Firehose (Throughput)
- **Minimum**: 500K EPS (events per second)
- **Target**: >1M EPS
- **Excellent**: >4M EPS

### Deep Dive (Memory Stability)
- **Assertion**: `memory_delta < 5MB`
- **Indicates**: O(1) memory usage during deep recursion

### Swarm (Parallel Scaling)
- **Minimum Scaling Efficiency**: 50% (2× speedup on 4 threads)
- **Target**: >80% (3.2× speedup on 4 threads)
- **Excellent**: >95% (3.8× speedup, near-linear)

## Artifact Management

### Uploaded Artifacts

1. **Benchmark Reports**
   - `benchmark-reports-Release/` - Release build results
   - `benchmark-reports-Debug/` - Debug build results
   - Includes: `.json`, `.csv`, `.txt` files

2. **Benchmark Results (Full)**
   - `benchmark-results-full/` - Complete benchmark data
   - Used for trend analysis across builds

### Accessing Artifacts

**In GitHub UI**:
1. Go to Actions → Workflow Run
2. Scroll to bottom → Artifacts section
3. Download desired artifact

**Via CLI**:
```bash
gh run download <run_id> -n benchmark-results-full
```

## PR Workflow

### For Contributors

1. **Create Branch**: `git checkout -b feat-my-feature`

2. **Push Changes**: 
   ```bash
   git push origin feat-my-feature
   ```

3. **Open PR**: Target `develop` or `main` branch

4. **Check CI Status**:
   - GitHub will automatically run all jobs
   - Status shown on PR page
   - Must pass before merge

5. **Review Benchmark Results**:
   - CI posts benchmark comparison to PR
   - Check for performance regressions
   - If baseline drops, investigate root cause

6. **Merge When Ready**:
   - All CI checks must pass
   - PR reviewers must approve
   - Can then merge with "Squash and merge"

### For Maintainers

**When Baseline Shifts**:

If benchmark results drop significantly:

1. Check PR for algorithmic changes
2. Run local benchmarks for comparison
3. Decide if change is acceptable or needs optimization
4. Add comment explaining reasoning
5. Adjust baseline if warranted

**Updating Baselines**:

If intentionally optimizing code and improving performance:

```bash
# Update docs with new baselines
# Push to develop branch
# Update this documentation file
```

## Failure Modes & Remediation

### Build Failure

**Symptoms**: "Build C++ Kernel" or "Build with Sanitizers" fails

**Remediation**:
1. Check compilation errors in job logs
2. Ensure all includes are present
3. Verify dependency versions
4. Test locally: `cmake --build . --config Release`

### Test Failure

**Symptoms**: Red "Run Unit Tests" status

**Remediation**:
1. Run locally: `ctest --output-on-failure`
2. Check test output for specific assertion failures
3. Fix underlying code bug
4. Re-push to trigger CI again

### Benchmark Regression

**Symptoms**: Benchmark throughput drops significantly

**Remediation**:
1. Check for algorithmic changes in the PR
2. Run local benchmarks for comparison
3. Profile with `perf` to identify bottleneck
4. Either optimize or explain regression in PR

### Memory Leak Detected

**Symptoms**: AddressSanitizer reports in job output

**Remediation**:
1. Check ASAN output for leak location
2. Examine code around reported line
3. Check for missing deallocations
4. Use `--leak-check=full` for detailed report
5. Fix leak and re-run

### Python/Node.js Binding Failure

**Symptoms**: "Smoke test" jobs fail

**Remediation**:
1. Check for C API changes
2. Ensure bindings handle new signatures
3. Test locally: `cd python && python example.py`
4. Update binding code if needed

## Monitoring & Analytics

### Build Trends

Access via GitHub:
- **Actions → Workflows → [Workflow Name] → Analytics**
- View pass/fail rates over time
- Identify patterns in failures

### Performance Trends

Create custom dashboard:
```bash
# Download all benchmark artifacts
for run in $(gh run list --limit 10 --json databaseId); do
  gh run download $run -n benchmark-results-full
done

# Analyze trend
python analyze_benchmarks.py
```

### Alerts & Notifications

Current setup uses GitHub's built-in notifications:
- Workflow failures email committer
- PR comments auto-notify reviewers

To add email alerts:
1. GitHub Settings → Notifications
2. Filter by repository
3. Enable per-rule notifications

## Environment Details

### GitHub Actions Runner

**OS**: Ubuntu Latest (Ubuntu 22.04 LTS at time of writing)

**Pre-installed Tools**:
- CMake 3.24+
- GCC/Clang with C++20 support
- Python 3.9+
- Node.js 18+
- Standard build tools

**Installed by Workflow**:
- `libatomic1` - Atomic operations library
- Custom dependencies in binding workflows

### Resource Limits

- **Timeout per Job**: 360 minutes (6 hours)
- **Timeout per Step**: 360 minutes
- **Disk Space**: ~25GB available
- **Memory**: ~7GB per job
- **CPU**: 2-core equivalent

## Security Considerations

### Secrets Management

No secrets are currently used. If adding API keys:

```yaml
- name: Deploy
  run: ./deploy.sh
  env:
    API_KEY: ${{ secrets.DEPLOY_API_KEY }}
```

Add via Settings → Secrets and Variables → Actions

### Dependency Security

Dependencies are:
- Pinned to known-good versions
- Installed via `apt-get` (Ubuntu package manager)
- Regularly updated via Dependabot (can be enabled)

To enable Dependabot:
1. GitHub → Settings → Code security
2. Enable "Dependabot version updates"
3. Create `.github/dependabot.yml` for policy

## Extending the Workflow

### Adding New Test Suite

```yaml
- name: Run My New Test
  working-directory: src/cpp_kernel/build
  run: ./my_test_binary
```

### Adding New Job

```yaml
my-new-job:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v3
    - name: My Step
      run: echo "Hello"
```

### Conditional Job Execution

Run only on main branch:
```yaml
if: github.ref == 'refs/heads/main'
```

Run only on PRs:
```yaml
if: github.event_name == 'pull_request'
```

## Local Reproduction

To reproduce CI behavior locally:

```bash
# Install dependencies
sudo apt-get update && sudo apt-get install -y cmake build-essential libatomic1

# Build Release
cd src/cpp_kernel
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run tests
ctest --output-on-failure

# Run benchmarks
./benchmark_harness --format=all

# Build with sanitizers
cd ../..
mkdir -p build-asan && cd build-asan
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_SANITIZERS=ON
cmake --build . --config Release
./mega_demo_asan
```

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Benchmark Harness Guide](./BENCHMARK_HARNESS.md)
- [Betti-RDL Architecture](../README.md)
- [Workflow YAML Specification](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions)

## Support & Troubleshooting

### Workflow Won't Trigger

1. Check branch name matches trigger conditions
2. Ensure `.github/workflows/ci.yml` exists
3. Check file for syntax errors (use `yamllint`)

### Job Hangs

- Set explicit timeout in job or step
- Check for infinite loops in benchmark
- Kill with timeout: `timeout 60 ./test`

### Intermittent Failures

- May indicate race condition in multi-threaded test
- Use Sanitizers to detect thread issues
- Run locally multiple times to reproduce

### Need Help?

Check:
1. GitHub Actions documentation
2. Workflow logs for specific error messages
3. Run commands locally to isolate issue
4. Create GitHub Issue with reproduction steps
