#!/usr/bin/env bash
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BUILD_DIR=$(mktemp -d)
cd "$BUILD_DIR" || exit 1

CIPD_ARGS=(
    -pkg-var "description:Protoc compiler"
    -install-mode copy
    -ref latest
)

PROTOC_URL="https://github.com/protocolbuffers/protobuf/releases/download/v21.1/protoc-21.1"

cd $(mktemp -d)
mkdir pkg
wget -q "$PROTOC_URL-win64.zip" -O "protoc.zip"
unzip -p "protoc.zip" "bin/protoc.exe" > "pkg/protoc.exe"
cipd create -in "pkg" -name "crosvm/protoc/windows-amd64" "${CIPD_ARGS[@]}"

cd $(mktemp -d)
mkdir pkg
wget -q "$PROTOC_URL-linux-x86_64.zip" -O "protoc.zip"
unzip -p "protoc.zip" "bin/protoc" > "pkg/protoc"
cipd create -in "pkg" -name "crosvm/protoc/linux-amd64" "${CIPD_ARGS[@]}"
