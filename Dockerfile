FROM ubuntu:24.04

COPY dist/sitecheck_*.deb /tmp/sitecheck.deb

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates \
      /tmp/sitecheck.deb curl \
 && rm -rf /var/lib/apt/lists/* /tmp/sitecheck.deb

ENV APP_PORT=8080
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -fsS http://127.0.0.1:8080/health || exit 1

USER 10001:10001
ENTRYPOINT ["sitecheck-server"]
