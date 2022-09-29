#!/bin/bash

printf "#include <mmx/mnemonic.h>\n\n" > out.cpp
printf "namespace mmx {\n" >> out.cpp
printf "namespace mnemonic {\n\n" >> out.cpp
printf "const std::vector<std::string> wordlist_en = {\n" >> out.cpp

while read word; do
	printf "\t\"$word\",\n" >> out.cpp
done < $1

printf "};\n}\n}" >> out.cpp
