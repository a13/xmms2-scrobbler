# filter out all tracks that don't have a musicbrainz artist id
class MusicBrainzFilter < SubmissionFilter
	def ignore?(propdict)
		(propdict[:track_id] || "").length.zero?
	end
end
