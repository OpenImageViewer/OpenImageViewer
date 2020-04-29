#Freetype Wrapper
#TODO: move source file under Source folder
 file(GLOB_RECURSE sourceFiles
     "./*.h"
     "./*.cpp"
 )

set(FreeTypePath D:/DevelopmentExternal/freetype2) # CACHE FORCE STRING
add_subdirectory(${FreeTypePath} ./external/freetype2)

set(TargetName FreeTypeWrapper)
add_library (${TargetName} ${sourceFiles} )
target_include_directories(${TargetName} PRIVATE ${FreeTypePath}/include)
target_include_directories(${TargetName} PRIVATE ../External/LLUtils/Include)
target_link_libraries(${TargetName} PRIVATE freetype) #${FreeTypePath}/objs/x64/Release Static/freetype.lib