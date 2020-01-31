#!/bin/sh
#
# Copyright 2020, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the GNU General Public License version 2. Note that NO WARRANTY is provided.
# See "LICENSE_GPLv2.txt" for details.
#
# @TAG(DATA61_GPL)
#

# Script to extract the aos sources from a repo managed repository into
# a single dir, and maintaining the correct symbolic links. If no
# directory exists a git repository is initialised. All updates to the
# directory are then added and the user is then prompted for a commit
# message.
#
# usage ./aos-make-project.sh <AOS_REPO_CHECKOUT> <NEW_DIR>
#

set -e

if [ $# -eq 1 ]; then
	DEST_REPO=$(realpath -m "$1"); shift
else
	echo "Usage: $0 REPOSITORY_DESITNATION" >&2
	exit 1
fi

SCRIPT_DIR=$(realpath -e "${0%/*}")

main () {
	src_repo=$(find_repo)
	cd "${src_repo}"

	clean_repo "${src_repo}"

	git_try_create "${DEST_REPO}"

	copy_to_repo \
		"${src_repo}" "${DEST_REPO}" "kernel/" \
		--links
	copy_to_repo \
		"${src_repo}" "${DEST_REPO}" "libnfs/" \
		--links --exclude ".gitignore"
	copy_to_repo \
		"${src_repo}" "${DEST_REPO}" "projects/" \
		--copy-links --exclude "aos-make-project.sh"
	copy_to_repo \
		"${src_repo}" "${DEST_REPO}" "tools/" \
		--links

	cd "${DEST_REPO}"
	ln \
		-srf \
		"projects/aos/reset.sh" \
		"${DEST_REPO}/reset.sh"
	ln \
		-srf \
		"projects/aos/init-build.sh" \
		"${DEST_REPO}/init-build.sh"
	ln \
		-srf \
		"projects/aos/.gitignore" \
		"${DEST_REPO}/.gitignore"
	ln \
		-srf \
		"tools/seL4/cmake-tool/default-CMakeLists.txt" \
		"${DEST_REPO}/CMakeLists.txt"

	git add .
	git commit -e
	echo "Repository updated in '${DEST_REPO}'"
}

# Find the root of the AOS repo.
find_repo () {
	dir=$SCRIPT_DIR

	while ! [ "${dir}" = "/" ] && ! [ -d "${dir}/.repo" ]; do
		dir=$(dirname "${dir}")
	done

	if [ ! -d "${dir}/.repo" ]; then
		echo "Failed to find root of AOS repo" >&2
		exit 1
	else
		echo "${dir}"
	fi
}

# Initialise the git repository if it doesn't exist
git_try_create () {
	repo_dir=$1; shift

	if [ -d "${repo_dir}/.git" ]; then
		echo "Adding to existing git repository \"${repo_dir}\"..."
		return
	fi

	echo "Creating new git repository in \"${repo_dir}\"..."
	git init "${repo_dir}"
}

# Copy files into a git repository
copy_to_repo () {
	src_dir=$1; shift
	dest_dir=$1; shift
	relative_path=$1; shift

	echo "Copying ${src_dir}/${relative_path} to ${dest_dir}/${relative_path}"

	rsync \
		-r \
		--exclude ".git" \
		"$@" \
		"${src_dir}/${relative_path}" \
		"${dest_dir}/${relative_path}"
}

# clean repo projects
clean_repo () {
	repo_dir=$1; shift
	cd "${repo_dir}"

	projects=$(repo list | cut -d ':' -f 1)

	for project in ${projects}; do
		echo "Cleaning '${repo_dir}/${project}'..."
		cd "${repo_dir}/${project}"
		git clean -fX
		git clean -i
	done
}

# Run the script
main
