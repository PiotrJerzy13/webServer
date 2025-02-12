.SILENT:

NAME = webServ

# Compiler and Flags
CXX = g++  # Use g++ for C++ files
CXXFLAGS = -Wall -Werror -Wextra -std=c++11 -I./inc

# Colors for output
GREEN = \033[0;32m
CYAN = \033[0;36m
WHITE = \033[0m

# Directories
SRC_DIR = ./src/
OBJ_DIR = ./objs/

# Source and Object files
SRC = $(SRC_DIR)main.cpp \
      $(SRC_DIR)parseConfig.cpp \

OBJ = $(addprefix $(OBJ_DIR), $(notdir $(SRC:.cpp=.o)))

# Rule to create the executable
$(NAME): $(OBJ)
	@$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJ) $(LDFLAGS)
	@echo "$(GREEN)$(NAME) built successfully!$(WHITE)"

# Rule to create object files from source files
$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Default target to build everything
all: $(NAME)

# Clean object files
clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(CYAN)Object files cleaned!$(WHITE)"

# Full clean, including the executable
fclean: clean
	@rm -f $(NAME)
	@echo "$(CYAN)Executable and object files cleaned!$(WHITE)"

# Rebuild everything
re: fclean all

.PHONY: all clean fclean re
