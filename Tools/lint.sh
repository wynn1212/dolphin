#! /bin/bash
#
# Linter script that checks for common style issues in Dolphin's codebase.

set -euo pipefail

if ! [ -x "$(command -v git)" ]; then
  echo >&2 "error: git is not installed"
  exit 1
fi

REQUIRED_CLANG_FORMAT_MAJOR=7
REQUIRED_CLANG_FORMAT_MINOR=0
CLANG_FORMAT=clang-format
CLANG_FORMAT_MAJOR=clang-format-${REQUIRED_CLANG_FORMAT_MAJOR}
CLANG_FORMAT_MAJOR_MINOR=${CLANG_FORMAT_MAJOR}.${REQUIRED_CLANG_FORMAT_MINOR}

if [ -x "$(command -v $CLANG_FORMAT_MAJOR)" ]; then CLANG_FORMAT=$CLANG_FORMAT_MAJOR; fi
if [ -x "$(command -v $CLANG_FORMAT_MAJOR_MINOR)" ]; then CLANG_FORMAT=$CLANG_FORMAT_MAJOR_MINOR; fi

if ! [ -x "$(command -v $CLANG_FORMAT)" ]; then
  echo >&2 "error: clang-format is not installed"
  echo >&2 "Install clang-format version ${REQUIRED_CLANG_FORMAT_MAJOR}.${REQUIRED_CLANG_FORMAT_MINOR}.*"
  exit 1
fi

FORCE=0

if [ $# -gt 0 ]; then
  case "$1" in
    -f|--force)
    FORCE=1
    shift
    ;;
  esac
fi

if [ $FORCE -eq 0 ]; then
  CLANG_FORMAT_VERSION=$($CLANG_FORMAT -version | cut -d' ' -f3)
  CLANG_FORMAT_MAJOR=$(echo $CLANG_FORMAT_VERSION | cut -d'.' -f1)
  CLANG_FORMAT_MINOR=$(echo $CLANG_FORMAT_VERSION | cut -d'.' -f2)

  if [ $CLANG_FORMAT_MAJOR != $REQUIRED_CLANG_FORMAT_MAJOR ] || [ $CLANG_FORMAT_MINOR != $REQUIRED_CLANG_FORMAT_MINOR ]; then
    echo >&2 "error: clang-format is the wrong version (${CLANG_FORMAT_VERSION})"
    echo >&2 "Install clang-format version ${REQUIRED_CLANG_FORMAT_MAJOR}.${REQUIRED_CLANG_FORMAT_MINOR}.* or use --force to ignore"
    exit 1
  fi
fi

did_java_setup=0
JAVA_CODESTYLE_FILE="./$(git rev-parse --show-cdup)/Source/Android/code-style-java.xml"
java_temp_dir=""

function java_setup() {
  if [ "$did_java_setup" = 1 ]; then
    return
  fi
  if [ ! -x "${ANDROID_STUDIO_ROOT}/bin/format.sh" ]; then
    echo >&2 "error: must set ANDROID_STUDIO_ROOT environment variable to the IDE installation directory (current: ${ANDROID_STUDIO_ROOT})"
    exit 1
  fi
  java_temp_dir="$(mktemp -d)"
  trap "{ rm -r ${java_temp_dir}; }" EXIT
  did_java_setup=1
}

fail=0

# Default to staged files, unless a commit was passed.
COMMIT=${1:---cached}

# Get modified files (must be on own line for exit-code handling)
modified_files=$(git diff --name-only --diff-filter=ACMRTUXB $COMMIT)

function java_check() {
  "${ANDROID_STUDIO_ROOT}/bin/format.sh" -s "${JAVA_CODESTYLE_FILE}" -R "${java_temp_dir}" >/dev/null

  # ignore 'added'/'deleted' files, we copied only files of interest to the tmpdir
  d=$(git diff --diff-filter=ad . "${java_temp_dir}" || true)
  if ! [ -z "${d}" ]; then
    echo "!!! Java code is not compliant to coding style, here is the fix:"
    echo "${d}"
    fail=1
  fi
}

# Loop through each modified file.
for f in ${modified_files}; do
  # Filter them.
  if echo "${f}" | egrep -q "[.]java$"; then
    # Copy Java files to a temporary directory
    java_setup
    mkdir -p $(dirname "${java_temp_dir}/${f}")
    cp "${f}" "${java_temp_dir}/${f}"
    continue
  fi
  if ! echo "${f}" | egrep -q "[.](cpp|h|mm)$"; then
    continue
  fi
  if ! echo "${f}" | egrep -q "^Source"; then
    continue
  fi

  # Check for clang-format issues.
  d=$($CLANG_FORMAT ${f} | (diff -u "${f}" - || true))
  if ! [ -z "${d}" ]; then
    echo "!!! ${f} not compliant to coding style, here is the fix:"
    echo "${d}"
    fail=1
  fi

  # Check for newline at EOF.
  last_line="$(tail -c 1 ${f})"
  if [ -n "${last_line}" ]; then
    echo "!!! ${f} not compliant to coding style:"
    echo "Missing newline at end of file"
    fail=1
  fi
done

if [ "${did_java_setup}" = 1 ]; then
  java_check
fi

exit ${fail}
