# the infamous Britney filter returns!
class BritneyFilter < SubmissionFilter
	def ignore?(propdict)
		(propdict[:artist] || "").match(/britney/i)
	end
end
