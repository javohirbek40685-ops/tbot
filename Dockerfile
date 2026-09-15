FROM ubuntu:22.04
RUN apt-get update && apt-get install -y g++ libcurl4-openssl-dev nlohmann-json3-dev make
WORKDIR /app
COPY . .
RUN g++ tbot.cpp -o tbot -lcurl
CMD ["./tbot"]
