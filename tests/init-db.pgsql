\set VERBOSITY     verbose
\set ON_ERROR_STOP on

drop database if exists c2h5oh_test__;
drop user if exists c2h5oh_test__;

create user c2h5oh_test__ password 'c2h5oh';

create database c2h5oh_test__ owner c2h5oh_test__;

\c c2h5oh_test__

\i ./tests/pq_test.pgsql
grant usage on schema pq_test to c2h5oh_test__;
grant all on table pq_test.t to c2h5oh_test__;

\i ./tests/web_test.pgsql
drop user if exists c2h5oh_web__;
create user c2h5oh_web__ password 'web';
grant usage on schema web to c2h5oh_web__;
grant execute on function web.route(varchar, jsonb, jsonb) to c2h5oh_web__;
