%% Logistic regression

clear;
close all;

disp('data');
parameter_names = [{'G_{Na}'},{'G_{NaB}'},{'V_{NaK}'},{'G_{Kr}'},{'G_{Kp}'},{'G_{ClCa}'},{'G_{ClB}'},{'G_{CaL}'},{'V_{NCX}'},{'V_{SERCA}'},{'V_{RyRRel}'},{'G_{Ks}'},{'G_{to}'},{'V_{RyRLeak}'},{'G_{Kur}'},{'G_{K1}'},{'G_{CaP}'},{'G_{CaB}'}];

all_parameters = load('para_log.dat.New');

rm_IKACH_parameters = all_parameters;%(:, [1:11, 13:25]);
%  x = reshape(load('../csv_1_old_scripts/ISO+50%kmf_EAD_DAD_count.csv'), 7, [])';
%  y = reshape(load('../csv_1_old_scripts/ISO+50%kmf_EAD_DAD_count.csv'), 7, [])';
 x = reshape(load('ISO+50%kmf_EAD_DAD_count.csv'), 7, [])';
 y = reshape(load('ISO+50%kmf_EAD_DAD_count.csv'), 7, [])';

% figname = 'ISO_0.1_CM_0_1_EAD.eps'
figname = 'ISO+50%kmf_EAD_DAD_count.eps'
logistic_EAD = (x(:,7)>=1);
logistic_DAD = (y(:,6)>=1);


abn_rep_count = sum(logistic_EAD)
ead_count = sum(logistic_EAD)
% normal_beat_count = sum(logistic_outputs(:,3))

% arrhythmia_presence = (logistic_EAD>1/2) | (logistic_DAD>1/2);
% arrhythmia_presence = (logistic_EAD>1/2) ;%| (logistic_DAD>1/2);
arrhythmia_presence =  (logistic_DAD>1/2);
arrhythmia_count = sum(arrhythmia_presence)

parameter_names_b0 = [{'b0'},parameter_names];

% allpars_LOGISTIC = all_parameters;


% allpars_LOGISTIC = [log(rm_IKACH_parameters),log(all_parameters(:,5)) .* log(all_parameters(:,17)) ];
X_LOG = log(rm_IKACH_parameters) ;

% X_LOG = allpars_LOGISTIC ;
[~,N_pars] = size(X_LOG)
for ii=1:N_pars % z-score
    X_LOGISTIC(:,ii)=(X_LOG(:,ii)-mean(X_LOG(:,ii)))/std(X_LOG(:,ii));
end

Y_LOGISTIC = 1-(arrhythmia_presence-1); % positive integer!
% arrhythmia_presence: 0 with normal beat, 1 with EADs/abnormal repolarization 
% Y_LOGISTIC: 1 with EADs/abnormal repolarization , 2 with normal beat % positive integer!

[B_LOGISTIC,dev,stats] = mnrfit(X_LOGISTIC,Y_LOGISTIC, 'interactions', 'on');
color = 'r'

figure; set(gcf,'color','w') % with b0
bar(B_LOGISTIC,'FaceColor',color)
set(gca,'box','off','tickdir','out','fontsize',12)
title('Probability arrhythmia development')
set(gca,'XTick',1:N_pars+1)
set(gca,'XTickLabel',parameter_names_b0)
set(gca,'XLim',[0 N_pars+1+1])
rotateXLabels( gca(), 90)

%% Arrhythmia Probability
% Parrhythmia in the baseline model
B0 = B_LOGISTIC(1);
pval_LOGISTIC = stats.p;

new_param_stat_significant = parameter_names_b0(pval_LOGISTIC<0.05);
disp('new_param_stat_significant:');
disp(new_param_stat_significant)
disp('\n')

disp('Parrhythmia in the baseline model:');
P_arrhythmia_mean_B0 = 1/(1+exp(-(B0)))
N_trials=600
% Parrhythmia in each model of the population
P_arrhythmia_array = zeros(1,N_trials);
for iii=1:N_trials
    P_arrhythmia_array(iii) = 1/(1+exp(-(B0+sum(B_LOGISTIC(2:end).*X_LOGISTIC(iii,:)'))));
end

% 
% color = ones(500,3);
% 
% color(arrhythmia_presence>0.5,:) = [1, 0, 0];
% color(arrhythmia_presence<0.5,:) = [0,1,0];

figure(2),set(gcf,'color','w')
for i = 1:N_trials
    if arrhythmia_presence(i) > 0.5 
        color = 'r';
    else
        color = 'b';
    end
    plot(i,P_arrhythmia_array(i),'*','Color',color), hold on;
end
set(gca,'box','off','tickdir','out','fontsize',12)
title('Probability arrhythmia development')
xlabel('Trial')
ylabel('Probability (-)')
P_ead_mean = mean(P_arrhythmia_array);
P_ead_std = std(P_arrhythmia_array);

array_1 = P_arrhythmia_array(arrhythmia_presence>0.5);
array_1_mean = mean(array_1);
array_1_std = std(array_1);
array_0 = P_arrhythmia_array(arrhythmia_presence<0.5);
array_0_mean = mean(array_0);
array_0_std = std(array_0);

% Tjur (2009)
R2logistic = array_1_mean - array_0_mean

P_predict = (P_arrhythmia_array>0.5);
% arrhythmia_presence
% P_predict == arrhythmia_presence
% P_predict' == arrhythmia_presence
mean(P_predict' == arrhythmia_presence)


[a1, I1] = sort(B_LOGISTIC(2:end));

para_names = parameter_names(I1);


figure(3); set(gcf,'color','w') % with b0
% bar(a_n,'FaceColor','b'); hold on;
bar(a1,'FaceColor','r');
set(gca,'box','off','tickdir','out','fontsize',12)
title('Probability arrhythmia development')

% N_pars = length(a_n_names);
set(gca,'XTick',1:N_pars)
set(gca,'XTickLabel',para_names)
set(gca,'XLim',[0 N_pars+1+1])
rotateXLabels( gca(), 90)


a_p = a1(a1>0);

a_p_names = para_names(a1>0);

[a_p, I1] = sort(a_p, 'descend');
a_p_names = a_p_names(I1);

a_n = a1(a1<0);
a_n_names = para_names(a1<0);

figure(4); set(gcf,'color','w') % with b0
bar(a_n,'FaceColor','#0072BD'); hold on;
% bar(a_p,'FaceColor','#D95319');
bar(a_p,'FaceColor','r');
set(gca,'box','off','tickdir','out','fontsize',12)
% title('Probability arrhythmia development')
ylabel('Regression Coefficient')
N_pars = length(a_n_names);
% set(gca,'XTick',1:N_pars)
% set(gca,'XTickLabel',a_n_names)
set(gca,'xtick',[])
ax1 = gca;                   % gca = get current axis
% ax1.YAxis.Visible = 'off';   % remove y-axis
ax1.XAxis.Visible = 'off';   % remove x-axis
xtickangle(90)
set(gca,'XLim',[0 N_pars+1])
% rotateXLabels( gca(), 90)
text((1:N_pars),a_n-0.2,a_n_names,'Rotation',90,'fontsize',12, 'HorizontalAlignment', 'right')

N_pars = length(a_p);
text((1:N_pars),a_p+0.2,a_p_names,'Rotation',90,'fontsize',12)
set(gca,'fontname','arial','fontsize',12)  % Set it to times
set(gca,'linewidth',1.5)
set(gcf,'Position',[100 100 550 450])
saveas(gcf,figname,'epsc')


Index = (P_arrhythmia_array'<0.5 & arrhythmia_presence>0.5);
figure,bar(rm_IKACH_parameters(Index,:)','grouped')
set(gca,'XTick',1:18)
set(gca,'XTickLabel',parameter_names)