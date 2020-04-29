#Freetype Wrapper
 file(GLOB_RECURSE sourceFiles
     "./*.h"
     "./*.cpp"
 )

set(TargetName easyexif)
add_library (${TargetName} ${sourceFiles} )