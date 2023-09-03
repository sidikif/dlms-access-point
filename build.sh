#/bin/bash
DOCKERBIN=docker
PROJECTNAME=dlms-access-point
REPO=git@github.com:sidikif/$PROJECTNAME.git#main
"$DOCKERBIN" build --target builder -t "sidikif/$PROJECTNAME-builder" "$REPO"
"$DOCKERBIN" build --target docserver -t "sidikif/$PROJECTNAME-docs" "$REPO:src/docker/docserver"
"$DOCKERBIN" build --target demo -t "sidikif/$PROJECTNAME" "$REPO:src/docker/demo"
"$DOCKERBIN" build --target dashboard -t "sidikif/$PROJECTNAME-dashboard" "$REPO:src/docker/dashboard"
