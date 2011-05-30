function [U, Hnew] = fallerEstimator(Y, X, H, mu)

% Near end signal is stacked frame by frame columnwise in matrix Y and far end in X
%
% Possible estimation procedures are
% 1) LSE
% 2) NLMS
% 3) Separated numerator and denomerator filters
regParam = 1;
[numFreqs, numFrames] = size(Y);
[numFreqs, Q] = size(X);
U = zeros(numFreqs, 1);

if ((nargin == 3) | (nargin == 5))
    dtd = 0;
end
if (nargin == 4)
    dtd = H;
end
Emax = 7;
dEH = Emax-sum(sum(H));
nu = 2*mu;
% if (nargin < 5)
%     H = zeros(numFreqs, Q);
%     for kk = 1:numFreqs
%         Xmatrix = hankel(X(kk,1:Q),X(kk,Q:end));
%         y = Y(kk,1:end-Q+1)';
%         H(kk,:) = (y'*Xmatrix')*inv(Xmatrix*Xmatrix'+regParam);
%         U(kk,1) = H(kk,:)*Xmatrix(:,1);
%     end
% else
    for kk = 1:numFreqs
        x = X(kk,1:Q)';
        y = Y(kk,1);
        Htmp = mu*(y-H(kk,:)*x)/(x'*x+regParam)*x;
        %Htmp = (mu*(y-H(kk,:)*x)/(x'*x+regParam) - nu/dEH)*x;
        H(kk,:) = H(kk,:) + Htmp';
        U(kk,1) = H(kk,:)*x;
    end
% end

Hnew = H;
