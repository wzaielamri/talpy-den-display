#!/bin/sh
# Builds and runs the host tests with gcc. Run from the repo root: sh test/native/run.sh
# Each test lists the pure-C sources it needs; add a run_test line for a new test file.

set -u
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
failed=0

run_test() {
    name=$1
    shift
    if ! gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -I src -I include -o "$out/$name" "test/native/$name.c" "$@"; then
        echo "FAIL (build): $name"
        failed=1
        return
    fi
    if "$out/$name"; then
        echo "ok: $name"
    else
        echo "FAIL: $name"
        failed=1
    fi
}

run_test test_codec src/ui/talpy_codec.c
run_test test_data src/ui/talpy_data.c src/ui/talpy_weather_data.c src/ui/talpy_codec.c
run_test test_player src/ui/talpy_player.c
run_test test_banner_text src/ui/banner_text.c
run_test test_weather_view src/ui/weather_view.c
run_test test_job_order src/ui/job_order.c
run_test test_job_watch src/app/job_watch.c

exit $failed
