cat <<EOF > tmp_dockerfile
FROM sphinxdoc/sphinx
RUN apt-get update -y
RUN apt-get install -y doxygen
COPY doc/requirements.txt /requirements.txt
RUN pip3 install -r /requirements.txt
EOF

docker build -f tmp_dockerfile -t local_pb_sphinx_docker $(pwd)/$(dirname $0)/..
rm tmp_dockerfile
docker run --rm -u $(id -u):$(id -g) -v $(pwd)/$(dirname $0)/..:/docs local_pb_sphinx_docker bash -c "cd doc && doxygen source/doxygen.cfg && sphinx-build -b html source _build/html"
