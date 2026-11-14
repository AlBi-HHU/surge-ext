ARG ALPINE_VERSION=3.23
FROM alpine:${ALPINE_VERSION} AS build

RUN apk add --no-cache build-base ca-certificates cmake curl zlib-dev

# The upstream ANU mirror has a complete TLS chain. Pin the archive checksum
# rather than disabling certificate verification on the other download host.
WORKDIR /opt/nauty
RUN curl --fail --location --retry 3 -o nauty.tar.gz \
        https://users.cecs.anu.edu.au/~bdm/nauty/nauty2_9_0.tar.gz \
      && echo "e2d79a341414944fc0c33ac611717cf32c9593748108aa9013dbf39a4f2a4b70  nauty.tar.gz" | sha256sum -c - \
      && tar xzf nauty.tar.gz --strip-components=1 \
      && ./configure --enable-generic CFLAGS="-O3" \
      && make -j"$(nproc)" nautyW1.a

WORKDIR /app
COPY CMakeLists.txt .
COPY src/ ./src/

RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DNATIVE_OPTIMIZATIONS=OFF \
      -DUSE_ZLIB=ON \
      -DNAUTY_DIR=/opt/nauty \
      -DNAUTY_LIB=/opt/nauty/nautyW1.a \
      && cmake --build build --parallel "$(nproc)"

FROM alpine:${ALPINE_VERSION} AS runtime
RUN apk add --no-cache zlib

COPY --from=build /app/build/surge-ext* /usr/local/bin/
WORKDIR /data

ENTRYPOINT ["surge-ext"]
CMD ["--help"]
