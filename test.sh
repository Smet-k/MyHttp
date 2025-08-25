PORT=${1:-8080}

for i in {1..50}; do
    curl http://localhost:$PORT & 
done

wait