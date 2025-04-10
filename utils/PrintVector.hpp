#ifndef __PRINT_VECTOR_HPP__
#define __PRINT_VECTOR_HPP__


#include <Arduino.h>
#include <vector>

// Helper function to print a vector of floats with indentation
void printVector(const std::vector<float>& vec, int indentLevel = 0) {
  // Print leading spaces for indentation
  for (int i = 0; i < indentLevel; ++i) {
    Serial.print("  ");  // Each indent level adds 2 spaces
  }

  Serial.print("{ ");
  for (size_t i = 0; i < vec.size(); ++i) {
    Serial.print(vec[i], 4);  // Print float with 4 decimal places
    if (i != vec.size() - 1) {
      Serial.print(", ");
    }
  }
  Serial.print(" }");
}

// Function to print a vector of vectors of floats with indentation
void printVectorOfVectors(const std::vector<std::vector<float>>& vec, int indentLevel = 0) {
  for (int i = 0; i < indentLevel; ++i) {
    Serial.print("  ");
  }
  Serial.println("{");
  for (size_t i = 0; i < vec.size(); ++i) {
    printVector(vec[i], indentLevel + 1);  // Indent inner vectors more
    if (i != vec.size() - 1) {
      Serial.print(", ");
    }
    Serial.println();  // Move to the next line after printing a vector
  }
  // Print the final closing brace with indentation
  for (int i = 0; i < indentLevel; ++i) {
    Serial.print("  ");
  }
  Serial.println("}");
}

// Function to print a vector of vectors of vectors of floats with indentation
void printVectorOfVectorOfVectors(const std::vector<std::vector<std::vector<float>>>& vec, int indentLevel = 0) {
  for (int i = 0; i < indentLevel; ++i) {
    Serial.print("  ");
  }
  Serial.println("{");
  for (size_t i = 0; i < vec.size(); ++i) {
    printVectorOfVectors(vec[i], indentLevel + 1);  // Indent inner vector of vectors more
    if (i != vec.size() - 1) {
      Serial.print(", ");
    }
    Serial.println();  // Move to the next line after printing a sub-vector of vectors
  }
  // Print the final closing brace with indentation
  for (int i = 0; i < indentLevel; ++i) {
    Serial.print("  ");
  }
  Serial.println("}");
}


#endif
