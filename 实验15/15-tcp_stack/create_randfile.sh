#!/bin/bash

dd if=/dev/urandom bs=40KB count=1 | base64 > client-input.dat
