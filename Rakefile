require "rake/clean"
require "rake/packagetask"
require "rake/contrib/sshpublisher"

PKG_NAME = "xmms2-scrobbler"
PKG_VERSION = File.read("xmms2-scrobbler").
              match(/^\s*VERSION = \"(.*)\"$/).captures.first
PKG_FILES = FileList[
	"AUTHORS", "COPYING", "README", "Rakefile", "xmms2-scrobbler",
	"filters/britney.rb", "filters/musicbrainz.rb", "ChangeLog"
]

CLOBBER.include("ChangeLog")

file "ChangeLog" do
	`git log > ChangeLog`
end

task :install do |t|
	destdir = ENV["DESTDIR"] || ""
	prefix = ENV["PREFIX"] || "/usr/local"

	ddir = File.join(destdir, prefix, "bin")

	FileUtils::Verbose.mkdir_p(ddir) unless File.directory?(ddir)
	FileUtils::Verbose.install("xmms2-scrobbler", ddir, :mode => 0755)

	ddir = File.join(destdir, prefix, "share/xmms2-scrobbler/filters")

	FileUtils::Verbose.mkdir_p(ddir) unless File.directory?(ddir)
	FileUtils::Verbose.install(Dir["filters/*.rb"], ddir, :mode => 0644)

end

Rake::PackageTask.new(PKG_NAME, PKG_VERSION) do |t|
	t.need_tar_gz = true
	t.package_files = PKG_FILES
end

task :publish => [:package] do
	Rake::SshFilePublisher.new("code-monkey.de", ".", "pkg",
	                           "#{PKG_NAME}-#{PKG_VERSION}.tar.gz").
	                       upload
end

