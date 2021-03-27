# frozen_string_literal: true

require "rmagick"
require "pry"

require_relative "../lib/colorama"

RSpec.describe Colorama do
  let(:rmagick_image) { Magick::Image.read("spec/example.jpg").first }

  it "has a version number" do
    expect(Colorama::VERSION).not_to be_a(Integer)
  end

  it "does something useful" do
    expect(rmagick_image).not_to be(nil)
    oi = Colorama::ColorExtractor.most_used_colors(rmagick_image, :low)

    expect(oi).to eq(
      background_color: "000000",
      detail_color: "d0aeee",
      primary_color: "14a98a",
      secondary_color: "ddb71b"
    )
  end
end
