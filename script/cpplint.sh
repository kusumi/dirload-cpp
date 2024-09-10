#!/bin/bash

# https://github.com/cpplint/cpplint

cpplint --filter=-whitespace/tab,-whitespace/comments,-legal/copyright src/* 2>&1
