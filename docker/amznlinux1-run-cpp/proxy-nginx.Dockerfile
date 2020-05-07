FROM openresty/openresty:1.15.8.1-3-alpine

# openresty packages extra ngx options like ngx.sleep

RUN apk add --update \
    curl \
    && rm -rf /var/cache/apk/*

ADD nginx.conf /usr/local/openresty/nginx/conf/nginx.conf

RUN chgrp -R 0 /usr/local/openresty/nginx/ && \
    chmod -R g=u /usr/local/openresty/nginx/

EXPOSE 9000