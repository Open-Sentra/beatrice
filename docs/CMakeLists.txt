# Documentation configuration for Beatrice SDK

# Find Doxygen
find_package(Doxygen)

if(DOXYGEN_FOUND)
    # Configure Doxyfile
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY
    )

    # Documentation target
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    # Install documentation
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/api
        FILES_MATCHING PATTERN "*.html"
        PATTERN "*.css"
        PATTERN "*.js"
        PATTERN "*.png"
        PATTERN "*.gif"
    )
else()
    message(WARNING "Doxygen not found. API documentation will not be generated.")
endif()

# Find Sphinx (for user guides)
find_package(Sphinx QUIET)

if(SPHINX_FOUND)
    # Sphinx documentation target
    add_custom_target(user_docs
        COMMAND ${SPHINX_EXECUTABLE} -b html
                ${CMAKE_CURRENT_SOURCE_DIR}/user_guide
                ${CMAKE_CURRENT_BINARY_DIR}/user_guide/html
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating user guide with Sphinx"
        VERBATIM
    )

    # Install user documentation
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_guide/html/
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/user_guide
        FILES_MATCHING PATTERN "*.html"
        PATTERN "*.css"
        PATTERN "*.js"
        PATTERN "*.png"
        PATTERN "*.gif"
    )
else()
    message(WARNING "Sphinx not found. User guide will not be generated.")
endif()

# Markdown documentation
file(GLOB_RECURSE MARKDOWN_FILES "*.md")
foreach(MD_FILE ${MARKDOWN_FILES})
    get_filename_component(MD_NAME ${MD_FILE} NAME_WE)
    get_filename_component(MD_DIR ${MD_FILE} DIRECTORY)
    
    # Convert markdown to HTML using pandoc if available
    find_program(PANDOC "pandoc")
    if(PANDOC)
        add_custom_target(${MD_NAME}_html
            COMMAND ${PANDOC} ${MD_FILE} -o ${CMAKE_CURRENT_BINARY_DIR}/${MD_NAME}.html
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Converting ${MD_FILE} to HTML"
            VERBATIM
        )
        
        # Install HTML documentation
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MD_NAME}.html
            DESTINATION ${CMAKE_INSTALL_DOCDIR}/markdown
        )
    endif()
    
    # Install markdown files
    install(FILES ${MD_FILE}
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/markdown
    )
endforeach()

# Install README and other documentation
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/../CHANGELOG.md
    ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# Create documentation index
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/index.html.in
    ${CMAKE_CURRENT_BINARY_DIR}/index.html
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/index.html
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
) 