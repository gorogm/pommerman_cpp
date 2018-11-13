#FROM python:3.6

FROM ubuntu:18.04
#RUN apt update
#RUN apt-get install -y software-properties-common vim
#RUN add-apt-repository ppa:jonathonf/python-3.6
RUN apt-get update && apt-get install -f -y python3.6 python3-pip gcc-7 g++-7 cmake
# To not have GL import errors:
RUN apt-get install -y libglu1-mesa-dev freeglut3-dev mesa-common-dev

#FROM alpine
#RUN apk add --update    python3 python3-dev py3-pip build-base && rm -rf /var/cache/apk/*

#RUN cat examples/docker-agent/run.py | grep agents | grep _agent > agentName
LABEL "agent"="cologne"
LABEL version="1.0"
LABEL releaseDate="2018.11.11."
LABEL maintainer="gorogm@gmail.com"

ADD playground/examples/docker-agent /agent

# @TODO to be replaced with `pip install pommerman`
ADD playground /pommerman
RUN cd /pommerman && pip3 install .
# end @TODO

ADD src /opt/work/pommermanmunchen/src
ADD CMakeLists.txt /opt/work/pommermanmunchen/CMakeLists.txt
RUN cd /opt/work/pommermanmunchen && mkdir cmake-build-debug && cd cmake-build-debug && cmake .. && make

EXPOSE 10080

ENV NAME Agent

# Run app.py when the container launches
WORKDIR /agent
ENTRYPOINT ["python3"]
CMD ["run_cologne.py"]
