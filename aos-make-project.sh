#!/bin/bash
#
# Script to extract the aos sources from a repo managed repository into a single
# dir, with no git or repo metadata, and maintaining the correct symlinks.
#
# usage ./aos-make-project.sh <AOS_REPO_CHECKOUT> <NEW_DIR>
#
# After running this script, you can simply initialise a new git repo in NEW_DIR
#

echo "Extracting AOS sources from $1 to $2"
rsync -rv --exclude ".git" --links $1/kernel $2/
rsync -rv --exclude ".git" --exclude ".gitignore" --links $1/libnfs $2/
rsync -rv --exclude ".git" --exclude "aos-make-project.sh" --copy-links $1/projects $2/
rsync -rv --exclude ".git" --links $1/tools $2/
cd $2
ln -sf projects/aos/reset.sh
ln -sf projects/aos/init-build.sh
ln -sf tools/seL4/cmake-tool/default-CMakeLists.txt CMakeLists.txt
cd ..
