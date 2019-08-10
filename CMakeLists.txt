cmake_minimum_required(VERSION 3.0)
project(export-fimfiction VERSION 0.1)

include(FindSQLite3)

add_compile_options(-fdiagnostics-color=always)

if(SQLite3_FOUND)
else()
	message(FATAL_ERROR "SQLite3 not found!")
endif()

include(FindPkgConfig)

function(external_package target prefix)
	target_include_directories("${target}" PUBLIC
		"${${prefix}_INCLUDE_DIRS}")
	target_compile_options("${target}" PUBLIC
		"${${prefix}_CFLAGS}")
	target_link_options("${target}" PUBLIC
		"${${prefix}_LDFLAGS}")
	target_link_libraries("${target}" PUBLIC
		"${${prefix}_LIBRARIES}")
endfunction()

add_subdirectory(libxmlfixes)

add_executable(export-fimfiction
  src/main.c)

pkg_check_modules(GIT
	REQUIRED
	libgit2)
external_package(generate GIT)

target_include_directories(generate PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/ddate")
target_link_libraries(generate PRIVATE storydb)

add_executable(ready
	src/ready.c
	cstuff/itoa.c)
target_link_libraries(ready PRIVATE storydb)
external_package(ready GIT)

add_executable(news_log
	src/news-log.c
	src/repo.c
	cstuff/become.c)
target_include_directories(news_log PRIVATE cstuff)
target_link_libraries(news_log PRIVATE storydb)
external_package(news_log GIT)

add_executable(make_log
	src/make-log.c
	src/repo.c
	cstuff/itoa.c
	cstuff/become.c)
target_include_directories(make_log PRIVATE cstuff)
target_link_libraries(make_log PRIVATE storydb xmlfixes)
external_package(make_log GIT)

set_source_files_properties(src/make-log.c
	PROPERTIES
	OBJECT_DEPENDS
	"template/make-log.html.c;template/make-log.row.html.c"
	)

add_executable(make_contents
	src/make-contents.c
	cstuff/itoa.c)
target_include_directories(make_contents PRIVATE cstuff)
target_link_libraries(make_contents PRIVATE storydb xmlfixes)
external_package(make_contents GIT)
set_source_files_properties(src/make-contents.c
	OBJECT_DEPENDS
	"template/page.html.c;template/contents-body.html.c;template/contents-story.html.c")

add_executable(describe
	src/describe.c
	cstuff/itoa.c
	src/db.c)
target_include_directories(describe PRIVATE cstuff)
target_link_libraries(describe PRIVATE storydb xml readline)
external_package(describe GIT)

add_executable(find_chapters_to_publish
	src/find-chapters-to-publish.c
	cstuff/itoa.c)
target_include_directories(find_chapters_to_publish PRIVATE cstuff)
target_link_libraries(find_chapters_to_publish PRIVATE storydb)
external_package(find_chapters_to_publish GIT)

##############################################33
# Now a bunch of generated files

add_subdirectory(str_to_enum_trie)
add_custom_command(OUTPUT category.gen.c category.gen.h
	COMMAND
	noupper=1 prefix=category enum=CATEGORY exec "./str_to_enum_trie/str_to_enum_trie" <"${CMAKE_CURRENT_SOURCE_DIR}/src/categories.list"
	MAIN_DEPENDENCY
	src/categories.list
	DEPENDS
	str_to_enum_trie/str_to_enum_trie)

add_executable(statements2source
	EXCLUDE_FROM_ALL
	src/statements2source.c
	cstuff/itoa.c
	cstuff/mmapfile.c)

external_package(statements2source SQLite3)
external_package(statements2source GIT)
target_include_directories(statements2source PUBLIC
  cstuff
  "${CMAKE_CURRENT_SOURCE_DIR}/htmlish/src")
target_link_libraries(statements2source PRIVATE htmlish justdb)

add_custom_command(
	OUTPUT db.sql.stmt.c
	COMMAND
	onlydefine=1
	"./statements2source"
	derp
	<"${CMAKE_CURRENT_SOURCE_DIR}/src/db.sql.stmt"
	>	"${CMAKE_CURRENT_BINARY_DIR}/db.sql.stmt.c"
	MAIN_DEPENDENCY
	"${CMAKE_CURRENT_SOURCE_DIR}/src/db.sql.stmt"
	DEPENDS
	statements2source)

set_source_files_properties(src/statements2source.c
	PROPERTIES
	OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/template/statements2source.c.c")

add_executable(dboid_make
	src/db_oid/make.c)

add_custom_command(
	OUTPUT db_oid/gen.h
	COMMAND
	"./dboid_make"
	db_oid "${CMAKE_CURRENT_SOURCE_DIR}/src/db_oid"
	DEPENDS
	dboid_make)
file(MAKE_DIRECTORY db_oid)

set_source_files_properties(src/db.c
	PROPERTIES
	OBJECT_DEPENDS
	"db_oid/gen.h;schema.sql.gen.c;indexes.sql.gen.c"
	)

# And now you learn the depths of my madness

file(MAKE_DIRECTORY template)
add_subdirectory(ctemplate)
function(ctemplate name)
	add_ctemplate(
		"${name}"
		"${CMAKE_CURRENT_SOURCE_DIR}/template/${name}"
		"template/${name}.c")
endfunction()

ctemplate(statements2source.c)
ctemplate(make-log.html)
ctemplate(make-log.row.html)
# for make-contents
ctemplate(page.html)
ctemplate(contents-body.html)
ctemplate(contents-story.html)

function(makeSQL name)
	set(src	"${CMAKE_CURRENT_SOURCE_DIR}/src/${name}.sql")
	set(dest "${name}.sql.gen.c")
	add_custom_command(
		OUTPUT "${dest}"
		COMMAND
		"./make_sql"
		<"${src}"
		>"${dest}"
		MAIN_DEPENDENCY
		"${src}"
		DEPENDS
		make_sql
		)
endfunction()

makeSQL(schema)
makeSQL(indexes)

add_executable(make_sql
	src/make-sql.c)

# str_to_enum_trie makes category.gen.c makes generate
# make-sql.c makes make-sql makes schema.sql.c makes justdb
# db_oid/make.c makes make_dboid makes db_oid/gen.h makes justdb
# ctemplate/ctemplate makes statements2source.c.c makes statements2source.c makes statements2source makes storydb
