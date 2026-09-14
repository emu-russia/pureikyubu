#!/bin/bash
# Startup-crash checks: each case must be reported by --selftest instead of taking the process down.
#
# The script deliberately corrupts the settings files of the build directory and puts them back at
# the end (a trap covers an interrupted run).
set -u
cd "$(dirname "$0")/../build"

cp Data/DefaultSettingsSdl.json /tmp/pureikyubu_def.bak
cp Data/SettingsSdl.json /tmp/pureikyubu_usr.bak

restore() {
  cp /tmp/pureikyubu_def.bak Data/DefaultSettingsSdl.json
  cp /tmp/pureikyubu_usr.bak Data/SettingsSdl.json
}
trap restore EXIT

failures=0

run() {
  local name="$1"
  local expected="$2"
  local arg="${3:-}"

  echo "$name"
  timeout 60 ./pureikyubu --selftest $arg > /tmp/pureikyubu_selftest.log 2>&1
  local code=$?

  tail -6 /tmp/pureikyubu_selftest.log
  echo "exit=$code"

  if [ "$code" -eq "$expected" ]; then
    echo "-> as expected"
  else
    echo "-> UNEXPECTED (expected exit $expected)"
    failures=$((failures + 1))
  fi
  echo
}

# A corrupt user settings file is reported and ignored, so the emulator still starts (exit 0).
printf '{' > Data/SettingsSdl.json
run "1. user settings truncated after the opening brace:" 0

printf '{"ui" "X":1}' > Data/SettingsSdl.json
run "2. user settings with a missing colon:" 0

printf '{"ui":{"PATH":"%s"}}' "$(head -c 6000 /dev/zero | tr '\0' 'A')" > Data/SettingsSdl.json
run "3. user settings with an over-long string:" 0

# The shipped defaults are not optional: without them the emulator cannot start.
printf '{"ui":' > Data/DefaultSettingsSdl.json
run "4. corrupt default settings (fatal):" 1
cp /tmp/pureikyubu_def.bak Data/DefaultSettingsSdl.json

head -c 4096 /dev/urandom > /tmp/pureikyubu_bad.dol
run "5. a random file renamed to .dol:" 1 /tmp/pureikyubu_bad.dol

head -c 64 /dev/urandom > /tmp/pureikyubu_bad.rvz
run "6. a random file renamed to .rvz:" 1 /tmp/pureikyubu_bad.rvz

cp /tmp/pureikyubu_usr.bak Data/SettingsSdl.json
run "7. restored settings (the emulator must start):" 0

if [ "$failures" -eq 0 ]; then
  echo "startup cases: all as expected"
else
  echo "startup cases: $failures unexpected result(s)"
fi

exit $failures
