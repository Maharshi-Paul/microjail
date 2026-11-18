.RECIPEPREFIX = >
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
SRC      = $(wildcard src/*.cpp)
OBJ      = $(SRC:.cpp=.o)
DEP      = $(OBJ:.o=.d)

microjail: $(OBJ)
> $(CXX) $(CXXFLAGS) -o $@ $(OBJ)

%.o: %.cpp
> $(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEP)

.PHONY: clean
clean:
> rm -f $(OBJ) $(DEP) microjail
