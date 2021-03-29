# frozen_string_literal: true

require "rmagick"

require_relative "../lib/colorama/color"

RSpec.describe Colorama::Color do
  subject(:color) { described_class.new_from_rgb(red, green, blue) }

  let(:red) { 12 }
  let(:green) { 14 }
  let(:blue) { 13 }

  describe '#hex' do
    subject(:hex) { color.hex }

    it { is_expected.to eq('0c0e0d') }
  end
end
