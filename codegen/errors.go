package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"text/template"
)

type ErrorEntry struct {
	Name        string `json:"name"`
	Description string `json:"description"`
}

type ErrorsFile struct {
	Errors []ErrorEntry `json:"errors"`
}

func loadTemplate(name string) *template.Template {
	tmpl, err := template.ParseFiles(filepath.Join("codegen", "templates", name))
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to load template %s: %v\n", name, err)
		os.Exit(1)
	}
	return tmpl
}

func generate(tmpl *template.Template, path string, data ErrorsFile) error {
	dir := filepath.Dir(path)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("failed to create directory %s: %v", dir, err)
	}

	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("failed to create %s: %v", path, err)
	}
	defer f.Close()

	if err := tmpl.Execute(f, data); err != nil {
		return fmt.Errorf("failed to execute template for %s: %v", path, err)
	}

	return nil
}

func main() {
	if len(os.Args) < 5 {
		fmt.Fprintf(os.Stderr, "Usage: %s <errors.json> <output.h> <output.cpp> <output.go>\n", os.Args[0])
		os.Exit(1)
	}

	jsonPath := os.Args[1]
	headerPath := os.Args[2]
	cppPath := os.Args[3]
	goPath := os.Args[4]

	data, err := os.ReadFile(jsonPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to read %s: %v\n", jsonPath, err)
		os.Exit(1)
	}

	var ef ErrorsFile
	if err := json.Unmarshal(data, &ef); err != nil {
		fmt.Fprintf(os.Stderr, "Failed to parse JSON: %v\n", err)
		os.Exit(1)
	}

	hTmpl := loadTemplate("errors.h.tmpl")
	cppTmpl := loadTemplate("errors.cpp.tmpl")
	goTmpl := loadTemplate("errors.go.tmpl")

	if err := generate(hTmpl, headerPath, ef); err != nil {
		fmt.Fprintf(os.Stderr, "%v\n", err)
		os.Exit(1)
	}

	if err := generate(cppTmpl, cppPath, ef); err != nil {
		fmt.Fprintf(os.Stderr, "%v\n", err)
		os.Exit(1)
	}

	if err := generate(goTmpl, goPath, ef); err != nil {
		fmt.Fprintf(os.Stderr, "%v\n", err)
		os.Exit(1)
	}

	fmt.Printf("Generated 3 files with %d errors\n", len(ef.Errors))
	fmt.Printf("  %s\n", headerPath)
	fmt.Printf("  %s\n", cppPath)
	fmt.Printf("  %s\n", goPath)
}
