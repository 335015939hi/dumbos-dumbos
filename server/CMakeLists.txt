
cmake_minimum_required(VERSION 4.0)

project(dumbos_server)


set (SOURCES
  main.c
  ../common.c
)


add_executable(${PROJECT_NAME} ${SOURCES})
