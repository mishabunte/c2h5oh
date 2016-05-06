create schema if not exists web;

-------------------------------------------------------------------------------
create or replace function web.route(u varchar, c jsonb, q jsonb) 
  returns text as 
$$
declare 
  ex_detail_ text;
  ex_context_ text;
  ex_hint_ text;
  res_ jsonb;
  function_ varchar;
begin
  execute 'select web.'||
    quote_ident(replace(regexp_replace(u, '^/?(.*?)/?$', '\1'), '/', '_'))||
    '($1,$2)' into res_ using c, q;
  --return tools.json_pp(res_::text);
  return res_;
exception 
  when undefined_function then
    raise warning '%', sqlerrm;
    return '{"status":404}';
  when sqlstate 'P0001' then
    return '{"content":{"status":"'||sqlerrm||'"}}';
  when others then
    get stacked diagnostics 
      ex_detail_ = pg_exception_detail,
      ex_context_ = pg_exception_context,
      ex_hint_ = pg_exception_hint;
    raise warning '% %', sqlerrm, ex_context_;
    return '{"content":{"status":"'||
                web.get_human_readable_sqlstate(sqlstate, sqlerrm)||'"}}';
end;
$$ language plpgsql security definer;

-------------------------------------------------------------------------------
create or replace function web.sum(c jsonb, q jsonb)
  returns text as
$$
-- Returns a + b
begin
  return json_build_object('content', json_build_object(
    'status', 'ok', 'sum', (q->>'a')::numeric + (q->>'b')::numeric));
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.cookie_set(c jsonb, q jsonb)
  returns text as
$$
-- Returns value as cookie header
begin
  return
    json_build_object('content', json_build_object('status', 'ok'),
      'headers', 
        'Set-Cookie: sid=' || (q->>'v') || '; Domain= .genosse.org; Expires=' ||
          to_char('2016-01-01'::timestamp + interval '14 days', 'Dy, DD-Mon-YYYY HH24:MI:SS GMT') || 
          '; Path=/; Secure; HttpOnly');
end;
$$
language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.cookie_get(c jsonb, q jsonb)
  returns text as
$$
-- Returns cookie value as content
begin
  return json_build_object('content', json_build_object(
    'status', 'ok', 'sid', c->>'sid'));
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.redirect(c jsonb, q jsonb)
  returns text as
$$
-- Returns redirect to specified url
begin
  return json_build_object('content', 'Redirected to: ' || (q->>'u'), 
    'status', 302, 'headers', 'Location: ' || (q->>'u'));
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.headers(c jsonb, q jsonb)
  returns text as
$$
-- Returns redirect to specified url
begin
  return json_build_object('content', 'ok', 
    'headers', json_build_array(
      'Header1: ' || (q->>'h1'), 
      'Header2: ' || (q->>'h2'), 
      'Content-Type: text/plain'
    ));
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create table if not exists web.login(id int not null);

-------------------------------------------------------------------------------
create or replace function web.user_login(c jsonb, q jsonb)
  returns text as
$$
begin
  insert into web.login (id) values ((q->>'token')::int);
  return '{"content":{"status":"ok"}}';
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.user_logout(c jsonb, q jsonb)
  returns text as
$$
begin
  truncate table web.login;
  return '{"content":{"status":"ok"}}';
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.user_auth(c jsonb, q jsonb)
  returns text as
$$
begin
  perform id from web.login where id = (c->>'sid')::int;
  if found then
    return '{"content":"OK"}';
  else
    return '{"status":403}';
  end if;
--exception when others then
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.large_json(c jsonb, q jsonb)
  returns text as
$$
begin
  return json_build_object('content', json_build_object(
    'status', 'ok', 'data', 
      (select json_agg(t.*) from 
        (select md5(''||random()), md5(''||random()) from 
           generate_series(0, (q->>'n')::int)) t)));
end;
$$ language plpgsql;

-------------------------------------------------------------------------------
create or replace function web.get_human_readable_sqlstate(varchar(5), varchar)
  returns varchar as
$$
begin
  return case 
    when $1='00000' then 'successful_completion'
    when $1='01000' then 'warning'
    when $1='0100C' then 'dynamic_result_sets_returned'
    when $1='01008' then 'implicit_zero_bit_padding'
    when $1='01003' then 'null_value_eliminated_in_set_function'
    when $1='01007' then 'privilege_not_granted'
    when $1='01006' then 'privilege_not_revoked'
    when $1='01004' then 'string_data_right_truncation'
    when $1='01P01' then 'deprecated_feature'
    when $1='02000' then 'no_data'
    when $1='02001' then 'no_additional_dynamic_result_sets_returned'
    when $1='03000' then 'sql_statement_not_yet_complete'
    when $1='08000' then 'connection_exception'
    when $1='08003' then 'connection_does_not_exist'
    when $1='08006' then 'connection_failure'
    when $1='08001' then 'sqlclient_unable_to_establish_sqlconnection'
    when $1='08004' then 'sqlserver_rejected_establishment_of_sqlconnection'
    when $1='08007' then 'transaction_resolution_unknown'
    when $1='08P01' then 'protocol_violation'
    when $1='09000' then 'triggered_action_exception'
    when $1='0A000' then 'feature_not_supported'
    when $1='0B000' then 'invalid_transaction_initiation'
    when $1='0F000' then 'locator_exception'
    when $1='0F001' then 'invalid_locator_specification'
    when $1='0L000' then 'invalid_grantor'
    when $1='0LP01' then 'invalid_grant_operation'
    when $1='0P000' then 'invalid_role_specification'
    when $1='0Z000' then 'diagnostics_exception'
    when $1='0Z002' then 'stacked_diagnostics_accessed_without_active_handler'
    when $1='20000' then 'case_not_found'
    when $1='21000' then 'cardinality_violation'
    when $1='22000' then 'data_exception'
    when $1='2202E' then 'array_subscript_error'
    when $1='22021' then 'character_not_in_repertoire'
    when $1='22008' then 'datetime_field_overflow'
    when $1='22012' then 'division_by_zero'
    when $1='22005' then 'error_in_assignment'
    when $1='2200B' then 'escape_character_conflict'
    when $1='22022' then 'indicator_overflow'
    when $1='22015' then 'interval_field_overflow'
    when $1='2201E' then 'invalid_argument_for_logarithm'
    when $1='22014' then 'invalid_argument_for_ntile_function'
    when $1='22016' then 'invalid_argument_for_nth_value_function'
    when $1='2201F' then 'invalid_argument_for_power_function'
    when $1='2201G' then 'invalid_argument_for_width_bucket_function'
    when $1='22018' then 'invalid_character_value_for_cast'
    when $1='22007' then 'invalid_datetime_format'
    when $1='22019' then 'invalid_escape_character'
    when $1='2200D' then 'invalid_escape_octet'
    when $1='22025' then 'invalid_escape_sequence'
    when $1='22P06' then 'nonstandard_use_of_escape_character'
    when $1='22010' then 'invalid_indicator_parameter_value'
    when $1='22023' then 'invalid_parameter_value'
    when $1='2201B' then 'invalid_regular_expression'
    when $1='2201W' then 'invalid_row_count_in_limit_clause'
    when $1='2201X' then 'invalid_row_count_in_result_offset_clause'
    when $1='22009' then 'invalid_time_zone_displacement_value'
    when $1='2200C' then 'invalid_use_of_escape_character'
    when $1='2200G' then 'most_specific_type_mismatch'
    when $1='22004' then 'null_value_not_allowed'
    when $1='22002' then 'null_value_no_indicator_parameter'
    when $1='22003' then 'numeric_value_out_of_range'
    when $1='22026' then 'string_data_length_mismatch'
    when $1='22001' then 'string_data_right_truncation'
    when $1='22011' then 'substring_error'
    when $1='22027' then 'trim_error'
    when $1='22024' then 'unterminated_c_string'
    when $1='2200F' then 'zero_length_character_string'
    when $1='22P01' then 'floating_point_exception'
    when $1='22P02' then 'invalid_text_representation'
    when $1='22P03' then 'invalid_binary_representation'
    when $1='22P04' then 'bad_copy_file_format'
    when $1='22P05' then 'untranslatable_character'
    when $1='2200L' then 'not_an_xml_document'
    when $1='2200M' then 'invalid_xml_document'
    when $1='2200N' then 'invalid_xml_content'
    when $1='2200S' then 'invalid_xml_comment'
    when $1='2200T' then 'invalid_xml_processing_instruction'
    when $1='23000' then 'integrity_constraint_violation'
    when $1='23001' then 'restrict_violation'
    when $1='23502' then 'not_null_violation'
    when $1='23503' then 'foreign_key_violation'
    when $1='23505' then 'unique_violation'
    when $1='23514' then 'check_violation'
    when $1='23P01' then 'exclusion_violation'
    when $1='24000' then 'invalid_cursor_state'
    when $1='25000' then 'invalid_transaction_state'
    when $1='25001' then 'active_sql_transaction'
    when $1='25002' then 'branch_transaction_already_active'
    when $1='25008' then 'held_cursor_requires_same_isolation_level'
    when $1='25003' then 'inappropriate_access_mode_for_branch_transaction'
    when $1='25004' then 'inappropriate_isolation_level_for_branch_transaction'
    when $1='25005' then 'no_active_sql_transaction_for_branch_transaction'
    when $1='25006' then 'read_only_sql_transaction'
    when $1='25007' then 'schema_and_data_statement_mixing_not_supported'
    when $1='25P01' then 'no_active_sql_transaction'
    when $1='25P02' then 'in_failed_sql_transaction'
    when $1='26000' then 'invalid_sql_statement_name'
    when $1='27000' then 'triggered_data_change_violation'
    when $1='28000' then 'invalid_authorization_specification'
    when $1='28P01' then 'invalid_password'
    when $1='2B000' then 'dependent_privilege_descriptors_still_exist'
    when $1='2BP01' then 'dependent_objects_still_exist'
    when $1='2D000' then 'invalid_transaction_termination'
    when $1='2F000' then 'sql_routine_exception'
    when $1='2F005' then 'function_executed_no_return_statement'
    when $1='2F002' then 'modifying_sql_data_not_permitted'
    when $1='2F003' then 'prohibited_sql_statement_attempted'
    when $1='2F004' then 'reading_sql_data_not_permitted'
    when $1='34000' then 'invalid_cursor_name'
    when $1='38000' then 'external_routine_exception'
    when $1='38001' then 'containing_sql_not_permitted'
    when $1='38002' then 'modifying_sql_data_not_permitted'
    when $1='38003' then 'prohibited_sql_statement_attempted'
    when $1='38004' then 'reading_sql_data_not_permitted'
    when $1='39000' then 'external_routine_invocation_exception'
    when $1='39001' then 'invalid_sqlstate_returned'
    when $1='39004' then 'null_value_not_allowed'
    when $1='39P01' then 'trigger_protocol_violated'
    when $1='39P02' then 'srf_protocol_violated'
    when $1='3B000' then 'savepoint_exception'
    when $1='3B001' then 'invalid_savepoint_specification'
    when $1='3D000' then 'invalid_catalog_name'
    when $1='3F000' then 'invalid_schema_name'
    when $1='40000' then 'transaction_rollback'
    when $1='40002' then 'transaction_integrity_constraint_violation'
    when $1='40001' then 'serialization_failure'
    when $1='40003' then 'statement_completion_unknown'
    when $1='40P01' then 'deadlock_detected'
    when $1='42000' then 'syntax_error_or_access_rule_violation'
    when $1='42601' then 'syntax_error'
    when $1='42501' then 'insufficient_privilege'
    when $1='42846' then 'cannot_coerce'
    when $1='42803' then 'grouping_error'
    when $1='42P20' then 'windowing_error'
    when $1='42P19' then 'invalid_recursion'
    when $1='42830' then 'invalid_foreign_key'
    when $1='42602' then 'invalid_name'
    when $1='42622' then 'name_too_long'
    when $1='42939' then 'reserved_name'
    when $1='42804' then 'datatype_mismatch'
    when $1='42P18' then 'indeterminate_datatype'
    when $1='42P21' then 'collation_mismatch'
    when $1='42P22' then 'indeterminate_collation'
    when $1='42809' then 'wrong_object_type'
    when $1='42703' then 'undefined_column'
    when $1='42883' then 'undefined_function'
    when $1='42P01' then 'undefined_table'
    when $1='42P02' then 'undefined_parameter'
    when $1='42704' then 'undefined_object'
    when $1='42701' then 'duplicate_column'
    when $1='42P03' then 'duplicate_cursor'
    when $1='42P04' then 'duplicate_database'
    when $1='42723' then 'duplicate_function'
    when $1='42P05' then 'duplicate_prepared_statement'
    when $1='42P06' then 'duplicate_schema'
    when $1='42P07' then 'duplicate_table'
    when $1='42712' then 'duplicate_alias'
    when $1='42710' then 'duplicate_object'
    when $1='42702' then 'ambiguous_column'
    when $1='42725' then 'ambiguous_function'
    when $1='42P08' then 'ambiguous_parameter'
    when $1='42P09' then 'ambiguous_alias'
    when $1='42P10' then 'invalid_column_reference'
    when $1='42611' then 'invalid_column_definition'
    when $1='42P11' then 'invalid_cursor_definition'
    when $1='42P12' then 'invalid_database_definition'
    when $1='42P13' then 'invalid_function_definition'
    when $1='42P14' then 'invalid_prepared_statement_definition'
    when $1='42P15' then 'invalid_schema_definition'
    when $1='42P16' then 'invalid_table_definition'
    when $1='42P17' then 'invalid_object_definition'
    when $1='44000' then 'with_check_option_violation'
    when $1='53000' then 'insufficient_resources'
    when $1='53100' then 'disk_full'
    when $1='53200' then 'out_of_memory'
    when $1='53300' then 'too_many_connections'
    when $1='53400' then 'configuration_limit_exceeded'
    when $1='54000' then 'program_limit_exceeded'
    when $1='54001' then 'statement_too_complex'
    when $1='54011' then 'too_many_columns'
    when $1='54023' then 'too_many_arguments'
    when $1='55000' then 'object_not_in_prerequisite_state'
    when $1='55006' then 'object_in_use'
    when $1='55P02' then 'cant_change_runtime_param'
    when $1='55P03' then 'lock_not_available'
    when $1='57000' then 'operator_intervention'
    when $1='57014' then 'query_canceled'
    when $1='57P01' then 'admin_shutdown'
    when $1='57P02' then 'crash_shutdown'
    when $1='57P03' then 'cannot_connect_now'
    when $1='57P04' then 'database_dropped'
    when $1='58000' then 'system_error'
    when $1='58030' then 'io_error'
    when $1='58P01' then 'undefined_file'
    when $1='58P02' then 'duplicate_file'
    when $1='F0000' then 'config_file_error'
    when $1='F0001' then 'lock_file_exists'
    when $1='HV000' then 'fdw_error'
    when $1='HV005' then 'fdw_column_name_not_found'
    when $1='HV002' then 'fdw_dynamic_parameter_value_needed'
    when $1='HV010' then 'fdw_function_sequence_error'
    when $1='HV021' then 'fdw_inconsistent_descriptor_information'
    when $1='HV024' then 'fdw_invalid_attribute_value'
    when $1='HV007' then 'fdw_invalid_column_name'
    when $1='HV008' then 'fdw_invalid_column_number'
    when $1='HV004' then 'fdw_invalid_data_type'
    when $1='HV006' then 'fdw_invalid_data_type_descriptors'
    when $1='HV091' then 'fdw_invalid_descriptor_field_identifier'
    when $1='HV00B' then 'fdw_invalid_handle'
    when $1='HV00C' then 'fdw_invalid_option_index'
    when $1='HV00D' then 'fdw_invalid_option_name'
    when $1='HV090' then 'fdw_invalid_string_length_or_buffer_length'
    when $1='HV00A' then 'fdw_invalid_string_format'
    when $1='HV009' then 'fdw_invalid_use_of_null_pointer'
    when $1='HV014' then 'fdw_too_many_handles'
    when $1='HV001' then 'fdw_out_of_memory'
    when $1='HV00P' then 'fdw_no_schemas'
    when $1='HV00J' then 'fdw_option_name_not_found'
    when $1='HV00K' then 'fdw_reply_handle'
    when $1='HV00Q' then 'fdw_schema_not_found'
    when $1='HV00R' then 'fdw_table_not_found'
    when $1='HV00L' then 'fdw_unable_to_create_execution'
    when $1='HV00M' then 'fdw_unable_to_create_reply'
    when $1='HV00N' then 'fdw_unable_to_establish_connection'
    when $1='P0000' then 'plpgsql_error'
    when $1='P0001' then $2
    when $1='P0002' then 'no_data_found'
    when $1='P0003' then 'too_many_rows'
    when $1='XX000' then 'internal_error'
    when $1='XX001' then 'data_corrupted'
    when $1='XX002' then 'index_corrupted'
    else 'unknown_error'
  end;
end;
$$ language plpgsql immutable;

