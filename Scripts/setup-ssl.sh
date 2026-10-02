#!/usr/bin/env bash

set -euo pipefail

# brew-installed OpenSSL on MacOS
PATH="/opt/homebrew/opt/openssl@3/bin:$PATH"

ensure=false
if [[ ${1:-} == --ensure ]]; then
    ensure=true
    shift
fi

n=${1:-4}
ssl_dir=${2:-"Player-Data"}
if [[ ! $n =~ ^[0-9]+$ ]] || (( $# > 2 )); then
    echo "Usage: $0 [--ensure] <nonnegative party count> [SSL directory]" >&2
    exit 2
fi
n=$((10#$n))

mkdir -p "$ssl_dir"

echo "Setting up SSL for $n parties"
rehash=true
if $ensure; then
    rehash=false
fi

for ((i = 0; i < n; i++)); do
    cert="$ssl_dir/P$i.pem"
    key="$ssl_dir/P$i.key"
    if $ensure && [[ -s $key ]] &&
        openssl x509 -in "$cert" -noout -checkend 86400 >/dev/null 2>&1; then
        cert_hash=$(openssl x509 -in "$cert" -noout -subject_hash)
        if [[ ! -e "$ssl_dir/$cert_hash.0" ]]; then
            rehash=true
        fi
        continue
    fi
    openssl req -newkey rsa -nodes -x509 -days 30 \
        -out "$cert" -keyout "$key" -subj "/CN=P$i"
    rehash=true
done

if $rehash; then
    c_rehash "$ssl_dir"
fi
