#!/bin/bash
set -e

# ==========================
# Step 1: Read input from user
# ==========================
read -p "Enter number of users (m): " USERS
read -p "Enter number of items (n): " ITEMS
read -p "Enter number of features (k): " FEATURES
read -p "Enter number of queries: " NUM_QUERIES

# Export them as environment variables
export USERS ITEMS FEATURES NUM_QUERIES

echo "Input summary: USERS=$USERS, ITEMS=$ITEMS, FEATURES=$FEATURES, NUM_QUERIES=$NUM_QUERIES"

# ==========================
# Step 2: Launch Docker Compose
# ==========================
docker-compose up --build
