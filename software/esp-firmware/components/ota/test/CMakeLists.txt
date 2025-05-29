
set(resources "")

list(APPEND resources "resources/createOTATask_version.json")
list(APPEND resources "resources/createOTATask_indicatesCorrectly.json")

list(APPEND resources "resources/processOTAAvailableFile_comments0.json")
list(APPEND resources "resources/processOTAAvailableFile_comments1.json")
list(APPEND resources "resources/processOTAAvailableFile_comments2.json")
list(APPEND resources "resources/processOTAAvailableFile_comments3.json")
list(APPEND resources "resources/processOTAAvailableFile_ignore1.json")
list(APPEND resources "resources/processOTAAvailableFile_invalid1.json")
list(APPEND resources "resources/processOTAAvailableFile_invalid2.json")
list(APPEND resources "resources/processOTAAvailableFile_invalid3.json")
list(APPEND resources "resources/processOTAAvailableFile_invalid4.json")
list(APPEND resources "resources/processOTAAvailableFile_invalid5.json")
list(APPEND resources "resources/processOTAAvailableFile_invalid6.json")
list(APPEND resources "resources/processOTAAvailableFile_string1.json")
list(APPEND resources "resources/processOTAAvailableFile_typical1.json")
list(APPEND resources "resources/processOTAAvailableFile_typical2.json")
list(APPEND resources "resources/processOTAAvailableFile_typical3.json")
list(APPEND resources "resources/processOTAAvailableFile_unordered1.json")

idf_component_register(SRC_DIRS "tests"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES ${resources}
                       REQUIRES unity
                                ota)