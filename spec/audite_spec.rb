require 'audite'

describe Audite do
  it "loads and reports on an mp3" do
    player = Audite.new
    player.load(File.dirname(__FILE__)+'/30sec.mp3')
    player.length.should eq(1326351)
    player.length_in_seconds.should eq(30.075986394557827)
  end
  it "generates an error on a non-existant mp3" do
    player = Audite.new
    expect {player.load('/tmp/file.mp3')}.to raise_error
  end
end
