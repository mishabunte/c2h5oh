#!/bin/bash
set -e
trim="tr -d '\n\r'"

# setup
./tests/init-db.sh >/dev/null
cd .build; mkdir -p c2h5oh_test; cd c2h5oh_test; rm -rf *.log; rm -rf uploads
trap '[ -f c2h5oh.pid ] && kill `cat c2h5oh.pid`' EXIT
[ -f c2h5oh.pid ] && kill `cat c2h5oh.pid`

# start c2h5oh
../nginx/objs/nginx -p . -c ../../tests/test-c2h5oh-nginx.conf &
sleep 0.1
c2h5oh_pid=`cat c2h5oh.pid`
[ "$c2h5oh_pid" -a "`ps -p $c2h5oh_pid`" ] || exit 1

function exit_error {
  echo "failed, result is:"; echo "[$res]"; cat error.log; exit 1;
}

# run tests

echo -n "test         GET ... "
res=$(curl -s 'http://localhost:10081/api/sum/?a=1.2&b=2.5'|jq -c -S '.')
[ "$res" = '{"status":"ok","sum":3.7}' ] || exit_error
echo "ok"

echo -n "test        POST ... "
res=$(curl -s -POST -d "a=1.2&b=2.6" 'http://localhost:10081/api/sum/'|jq -c -S '.')
[ "$res" = '{"status":"ok","sum":3.8}' ] || exit_error
echo "ok"

echo -n "test  cookie_set ... "
res=$(curl -i -s 'http://localhost:10081/api/cookie/set/?v=777'|grep 'Set-Cookie'|$trim)
[ "$res" = 'Set-Cookie: sid=777; Domain= .genosse.org; Expires=Fri, 15-Jan-2016 00:00:00 GMT; Path=/; Secure; HttpOnly' ] || exit_error
echo "ok"

echo -n "test  cookie_get ... "
res=$(curl -s --cookie 'sid=666' 'http://localhost:10081/api/cookie/get/'|jq -c -S '.')
[ "$res" = '{"sid":"666","status":"ok"}' ] || exit_error
echo "ok"

echo -n "test    redirect ... "
res=$(curl -i -s 'http://localhost:10081/api/redirect/?u=http://genosse.org/'|grep 'Location'|$trim)
[ "$res" = 'Location: http://genosse.org/' ] || exit_error
echo "ok"

echo -n "test     headers ... "
res=$(curl -i -s 'http://localhost:10081/api/headers/?h1=100&h2=500'|grep 'Header'|$trim)
[ "$res" = 'Header1: 100Header2: 500' ] || exit_error
echo "ok"

echo -n "test       login ... "
res=$(curl -s 'http://localhost:10081/api/user/login/?token=42'|jq -c -S '.')
[ "$res" = '{"status":"ok"}' ] || exit_error
res=$(curl -i -s --cookie 'sid=43' 'http://localhost:10081/api/user/auth/'|head -n1|$trim)
[ "$res" = 'HTTP/1.1 403 Forbidden' ] || exit_error
res=$(curl -i -s --cookie 'sid=42' 'http://localhost:10081/api/user/auth/'|head -n1|$trim)
[ "$res" = 'HTTP/1.1 200 OK' ] || exit_error
echo "ok"

echo -n "test upload auth ... "
res=$(curl -POST -d@../../README -i -s 'http://localhost:10081/api/upload/'|head -n1|$trim)
[ "$res" = 'HTTP/1.1 403 Forbidden' ] || exit_error
res=$(curl -s -POST --data-binary @../../README -H 'Content-Type: application/octet-stream' \
  --cookie 'sid=42' 'http://localhost:10081/api/upload/'|jq -r '.name')
[ "$res" ] || exit_error
res=$(curl -s "http://localhost:10081/uploads/$res" | md5sum)
[ "$(cat ../../README | md5sum)" = "$res" ] || exit_error
echo "ok"

echo -n "test      logout ... "
res=$(curl -i -s --cookie 'sid=42' 'http://localhost:10081/api/user/auth/'|head -n1|$trim)
[ "$res" = 'HTTP/1.1 200 OK' ] || exit_error
res=$(curl -s 'http://localhost:10081/api/user/logout/'|jq -c -S '.')
[ "$res" = '{"status":"ok"}' ] || exit_error
res=$(curl -i -s --cookie 'sid=42' 'http://localhost:10081/api/user/auth/'|head -n1|$trim)
[ "$res" = 'HTTP/1.1 403 Forbidden' ] || exit_error
echo "ok"

echo -n "test  large_json ... "
res=$(curl -s 'http://localhost:10081/api/large_json/?n=9000'|jq -c -S 'del(.data)')
[ "$res" = '{"status":"ok"}' ] || exit_error
echo "ok"
echo ""

#cat error.log
#cat access.log

$CTEST_INTERACTIVE_DEBUG_MODE || exit 0
read -n1 -r -p "Press any key to stop..." 


