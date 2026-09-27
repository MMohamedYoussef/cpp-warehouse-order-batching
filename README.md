# Warehouse order batching

This C++17 console program groups warehouse picks into cart trips and compares walking distance with one trip per order. It helps a small warehouse estimate the benefit of batching before adopting a route planner.

Build with a C++17 compiler (standard library only):

```sh
c++ -std=c++17 -Wall -Wextra -Wpedantic main.cpp -o batch
./batch fixtures/orders.csv 3 example-out.csv
python3 tests/test_cli.py ./batch
```

Input is unquoted CSV with the exact header `order_id,location,units`. Each order ID appears once; `location` is nonnegative distance along a single aisle from the depot, and `units` is a positive integer no greater than cart capacity. The program sorts picks by location (then ID), adds them to the current cart until the next pick will not fit, and starts another trip. Every trip returns to the depot, so its distance is twice its farthest location. This next-fit heuristic is deterministic and capacity-safe; it does not claim a globally shortest batching plan.

The output file has `batch_id,orders,units,walking_distance`; `orders` joins IDs with `|`. The demo produces [examples/batches.csv](examples/batches.csv) and prints `batches=2 baseline_distance=28 batched_distance=24`. Order IDs beginning with `=`, `+`, `-`, or `@` after leading whitespace are rejected to keep exported CSV safe to open in a spreadsheet. Output is staged and published only if the requested path is absent; this requires hard-link support in the output directory. Input is limited to 100,000 orders and location/units/capacity values of at most 1,000,000,000. Quoted CSV and multi-line IDs are unsupported; malformed input exits nonzero with a diagnostic.
