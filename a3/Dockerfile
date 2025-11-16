FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ make cmake \
    libboost-system-dev \
    libssl-dev \
    && apt-get clean

WORKDIR /app
COPY . .

# Build everything
RUN make clean && make all

# Default command: let docker-compose override it
CMD ["/bin/bash"]
