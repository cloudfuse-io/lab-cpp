# FROM httpd:2.4

# RUN apt-get update && apt-get install -y curl
# COPY httpd.conf /usr/local/apache2/conf/httpd.conf

FROM nginx

RUN apt-get update && apt-get install -y curl
COPY nginx.conf /etc/nginx/conf.d/default.conf

EXPOSE 9000