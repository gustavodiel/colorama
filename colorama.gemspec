# frozen_string_literal: true

require_relative "lib/colorama/version"

Gem::Specification.new do |spec|
  spec.name          = "colorama"
  spec.version       = Colorama::VERSION
  spec.authors       = ["Gustavo Diel"]
  spec.email         = ["gustavodiel@hotmail.com"]

  spec.summary       = "A Gem that extracts some useful colors from an image"
  spec.homepage      = "https://github.com/gustavodiel/colorama"
  spec.license       = "MIT"
  spec.required_ruby_version = Gem::Requirement.new(">= 2.4.0")

  spec.metadata["allowed_push_host"] = "https://rubygems.org"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = "https://github.com/gustavodiel/colorama"
  spec.metadata["changelog_uri"] = "https://github.com/gustavodiel/colorama/blob/master/CHANGELOG.md"

  spec.files = Dir.chdir(File.expand_path(__dir__)) do
    `git ls-files -z`.split("\x0").reject { |f| f.match(%r{\A(?:test|spec|features)/}) }
  end
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
end
