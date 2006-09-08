# the infamous Britney filter returns!
class BritneyFilter < SubmissionFilter
	def ignore?(m)
		m[:artist].match(/britney/i)
	end
end
