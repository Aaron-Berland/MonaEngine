function(Add_Example TARGETNAME FILENAME)
	add_executable(${TARGETNAME} ${FILENAME})
	set_property(TARGET ${TARGETNAME} PROPERTY CXX_STANDARD 20)
	set_property(TARGET ${TARGETNAME} PROPERTY FOLDER Examples)
	target_link_libraries(${TARGETNAME} PRIVATE MonaEngine)
	target_include_directories(${TARGETNAME} PRIVATE ${MONA_INCLUDE_DIRECTORY} ${THIRD_PARTY_INCLUDE_DIRECTORIES})

endfunction(Add_Example)
Add_Example(Example0_MinimalSetup MinimalSetup.cpp)
Add_Example(Example1_WindowInputOperations WindowInputExample.cpp)
Add_Example(Example2_Breakout Breakout.cpp)
Add_Example(Example3_AnimationAudio AnimationAudioExample.cpp)


