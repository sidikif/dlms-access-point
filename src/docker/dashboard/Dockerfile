FROM nginx:alpine AS dashboard
COPY --from=sidikif/dlms-access-point-builder /tmp/build/doc/html/dashboard /usr/share/nginx/html
COPY --from=sidikif/dlms-access-point-builder /tmp/src/docker/docserver/default.conf /etc/nginx/conf.d/default.conf
