FROM httpd:2.4

RUN apt-get update && apt-get install -y curl
COPY httpd.conf /usr/local/apache2/conf/httpd.conf

EXPOSE 9000