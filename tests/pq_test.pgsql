\set ON_ERROR_STOP on
create schema if not exists pq_test;

create table if not exists pq_test.t (id int, val int);

-------------------------------------------------------------------------------
create or replace function pq_test.pq_test(q jsonb) 
returns json as 
$$
begin
  if (q ? 'error') then
    raise sqlstate '00001';
  end if;
  if (q ? 'sleep') then
    perform pg_sleep((q->>'sleep')::real);
  end if;
  if (q ? 'null') then
    return null;
  end if;
  return json_build_object('sum', (q->>'a')::int + (q->>'b')::int);
end;
$$
language plpgsql;

-------------------------------------------------------------------------------
create or replace function pq_test.tests() 
returns varchar as
$$
declare
  error_ boolean;
  tm_    timestamp;
begin
  begin
    error_ := true;
    perform pq_test.pq_test('{"error":true}');
  exception when sqlstate '00001' then
    error_ := false; 
  end;
  if error_ then 
    raise 'error not handled';
  end if;

  if (pq_test.pq_test('{"a" : 1, "b" : 2}')->>'sum')::int <> 3 then
    raise 'wrong sum';
  end if;


  tm_ := clock_timestamp();
  perform pq_test.pq_test('{"a" : 1, "b" : 2, "sleep" : 0.1 }');
  if clock_timestamp() - tm_ < interval '0.1' then
    raise 'wrong time';
  end if;

  if pq_test.pq_test('{"null" : true}') is not null then
    raise 'null failed';
  end if;

  return 'passed';

end;
$$ 
language plpgsql;

-------------------------------------------------------------------------------
select pq_test.tests();
