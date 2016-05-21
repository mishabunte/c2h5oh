#!/bin/sh
psql -f ./tests/init-db.pgsql -v ON_ERROR_STOP=1
