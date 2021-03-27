# frozen_string_literal: true

require 'rmagick'

require_relative "colorama/version"
require_relative "colorama/color"
require_relative "colorama/color_extractor"

module Colorama
  QUALITIES = %i[lowest low high highest]

  def self.extract_from_file(image_file, quality: :highest)
    raise ArgumentError("quality must be: #{QUALITIES}") unless QUALITIES.include?(quality)

    image = ::Magick::Image.read(image_file).first

    Colorama::ColorExtractor.most_used_colors(image, quality)
  end
end
