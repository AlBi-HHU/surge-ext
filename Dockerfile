############################
# Build stage (Alpine)
############################
FROM alpine:3.19 AS build

# Installiere Build-Tools und zlib-Header für Alpine
RUN apk add --no-cache build-base cmake curl zlib-dev

# ---- build nauty ----
WORKDIR /opt/nauty
RUN curl -L -o nauty.tar.gz https://pallini.di.uniroma1.it/nauty2_9_0.tar.gz \
      && tar xzf nauty.tar.gz --strip-components=1 \
      && ./configure && make

ENV NAUTY_HOME=/opt/nauty

# ---- project sources ----
WORKDIR /app

COPY CMakeLists.txt .
COPY src/ ./src/

RUN mv ${NAUTY_HOME} /app/nauty

# ---- build project ----
RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_VERBOSE_MAKEFILE=ON \
      -DNAUTY_DIR="${NAUTY_HOME}" \
      -DNAUTY_LIB="${NAUTY_HOME}/nautyW1.a" 

RUN cmake --build build -j$(nproc)

############################
# Runtime stage (Alpine)
############################
FROM alpine:3.19 AS runtime

COPY --from=build /app/build/surge-ext* /usr/local/bin/

ENTRYPOINT ["surge-ext"]
CMD ["--help"]