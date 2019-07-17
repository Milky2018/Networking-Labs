#!/bin/bash

dd if=/dev/urandom bs=60KB count=1 | base64 > client-input.dat
