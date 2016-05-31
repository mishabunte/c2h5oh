create schema v1;
create function v1.dosum(a int4, b int4) returns int4 as $$
begin
  return dosum.a + dosum.b;
end;
$$ language plpgsql;
