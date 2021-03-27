# frozen_string_literal: true

require "rmagick"

require_relative "../lib/colorama/color"

RSpec.describe Colorama::Color do
  subject { described_class.new_from_rgb(red, green, blue) }
  let(:red) { 255 }
  let(:green) { 255 }
  let(:blue) { 255 }
end
