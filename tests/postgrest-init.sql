create schema v1;
create function v1.dosum(int4, int4) returns int4 as $$
  select $1 + $2;
$$ language sql;
