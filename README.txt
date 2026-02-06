#bash 
mkdir build && cd build
cmake ..
make -j
cp aggregator ../docker_build/

cd ../docker_build
#docker build -t aggregator -f Dockerfile .
docker build -t aggregator .
docker run --rm -p 50051:50051 aggregator
#or backend
docker run -d --rm -p 50051:50051 --name aggregator aggregator

#if "failed: port is already allocated", kill the process on that port or use another port

#check running docker
docker ps 

#stop docker 
docker stop <CONTAINER ID>

#delete all images
docker images -aq | xargs -r docker rmi -f

